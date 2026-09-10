// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "base/base_paths.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/file_util_icu.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/threading/thread_restrictions.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/model/web_app_icon_types.h"
#include "chrome/browser/web_applications/os_integration/os_integration_test_override.h"
#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/test/fake_environment.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#if BUILDFLAG(IS_LINUX)
#include "base/nix/xdg_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <ImageIO/ImageIO.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#include "chrome/browser/web_applications/os_integration/mac/bundle_info_plist.h"
#include "chrome/browser/web_applications/os_integration/mac/web_app_shortcut_mac.h"
#include "net/base/filename_util.h"
#import "skia/ext/skia_utils_mac.h"
#endif

namespace web_app {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Performs a blocking read of app icons from the disk.
std::optional<SkBitmap> IconManagerReadIconForSize(
    WebAppIconManager& icon_manager,
    const webapps::AppId& app_id,
    SquareSizePx size_px) {
  if (!icon_manager.HasIcons(app_id, IconPurpose::ANY, {size_px})) {
    return std::nullopt;
  }
  std::optional<SkBitmap> result;
  base::RunLoop run_loop;
  icon_manager.ReadTrustedIconsWithFallbackToManifestIcons(
      app_id, {size_px}, IconPurpose::ANY,
      base::BindLambdaForTesting([&](IconMetadataFromDisk icon_metadata) {
        OrderedSizeToBitmap icon_bitmaps = std::move(icon_metadata.icons_map);
        CHECK(icon_bitmaps.contains(size_px));
        result = icon_bitmaps[size_px];
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}
#endif

#if BUILDFLAG(IS_MAC)
// Note: This signature matches the one below for Windows.
// TODO(https://crbug.com/385198233): Split the files entirely by platform.
std::optional<SkBitmap> GetIconFromShortcutFile(
    const base::FilePath& shortcut_path) {
  CHECK(base::PathExists(shortcut_path));
  base::FilePath icon_path =
      shortcut_path.AppendASCII("Contents/Resources/app.icns");
  base::apple::ScopedCFTypeRef<CFDictionaryRef> empty_dict(
      CFDictionaryCreate(nullptr, nullptr, nullptr, 0, nullptr, nullptr));
  base::apple::ScopedCFTypeRef<CFURLRef> url =
      base::apple::FilePathToCFURL(icon_path);
  base::apple::ScopedCFTypeRef<CGImageSourceRef> source(
      CGImageSourceCreateWithURL(url.get(), nullptr));
  if (!source) {
    return std::nullopt;
  }
  // Get the first icon in the .icns file (index 0)
  base::apple::ScopedCFTypeRef<CGImageRef> cg_image(
      CGImageSourceCreateImageAtIndex(source.get(), 0, empty_dict.get()));
  if (!cg_image) {
    return std::nullopt;
  }
  SkBitmap bitmap = skia::CGImageToSkBitmap(cg_image.get());
  if (bitmap.empty()) {
    return std::nullopt;
  }
  return bitmap;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace

OsIntegrationTestOverrideBlockingRegistration::
    OsIntegrationTestOverrideBlockingRegistration() {
  scoped_refptr<OsIntegrationTestOverride> test_override =
      OsIntegrationTestOverride::GetOrCreateForBlockingRegistration([]() {
        base::FilePath base_path;
#if BUILDFLAG(IS_MAC)
        // Mac app shims must be put within the user's home directory to allow
        // LaunchServices to index it. Otherwise, while launching may succeed,
        // some functionality like file handling does not work correctly.
        base_path = base::GetHomeDir();
#endif
        return base::WrapRefCounted<OsIntegrationTestOverride>(
            new OsIntegrationTestOverrideImpl(base_path));
      });
  test_override_ =
      base::WrapRefCounted(test_override->AsOsIntegrationTestOverrideImpl());
}

OsIntegrationTestOverrideBlockingRegistration::
    ~OsIntegrationTestOverrideBlockingRegistration() {
  base::ScopedAllowBlockingForTesting blocking;
  std::optional<base::RunLoop> wait_until_destruction_loop;

  // Safely decrement the blocking registration refcount, and if this was the
  // last one, clear the global state & listen for destruction of all overrides.
  // We want to wait for all overrides to destroy as this cleans up the OS
  // integration disk state.
  {
    base::AutoLock lock(test_override_->destruction_closure_lock_);
    CHECK(!test_override_->on_destruction_)
        << "Cannot have multiple registrations waiting for destruction at the "
           "same time, only the last one should.";
    bool is_last_registration = OsIntegrationTestOverride::
        DecreaseBlockingRegistrationCountMaybeReset();
    if (is_last_registration) {
      // This object can be destroyed after the task environment has already
      // been destroyed in tests. If that's the case, we cannot create a
      // base::RunLoop and we can simply destruct.
      if (base::SequencedTaskRunner::HasCurrentDefault()) {
        wait_until_destruction_loop.emplace();
        test_override_->on_destruction_.ReplaceClosure(
            wait_until_destruction_loop->QuitClosure());
      } else {
        // This should be the last reference if there is no task environment and
        // this was the last registration. If this fails, that means that a test
        // or something else has saved a scoped_refptr to the
        // OsIntegrationTestOverride and hasn't released it before destroying
        // the registration. Since there is no task runner, this is likely in
        // the test harness being destroyed after the registration (and after
        // the task environment).
        CHECK(test_override_->HasOneRef());
      }
    }
  }

  // Release the override & wait until all references are released.
  // Note: The `test_override` MUST be released before waiting on the run
  // loop, as then it will hang forever.
  test_override_.reset();
  if (wait_until_destruction_loop) {
    wait_until_destruction_loop->Run();
  }
}

OsIntegrationTestOverrideImpl&
OsIntegrationTestOverrideBlockingRegistration::test_override() const {
  return *OsIntegrationTestOverrideImpl::Get();
}

// static
scoped_refptr<OsIntegrationTestOverrideImpl>
OsIntegrationTestOverrideImpl::Get() {
  CHECK_IS_TEST();
  scoped_refptr<OsIntegrationTestOverride> current_override =
      OsIntegrationTestOverride::Get();
  CHECK(current_override);
  scoped_refptr<OsIntegrationTestOverrideImpl> test_override =
      base::WrapRefCounted<OsIntegrationTestOverrideImpl>(
          current_override->AsOsIntegrationTestOverrideImpl());
  CHECK(test_override);
  return test_override;
}

// static
std::unique_ptr<OsIntegrationTestOverrideImpl::BlockingRegistration>
OsIntegrationTestOverrideImpl::OverrideForTesting() {
  return std::make_unique<
      OsIntegrationTestOverrideImpl::BlockingRegistration>();
}

bool OsIntegrationTestOverrideImpl::SimulateDeleteShortcutsByUser(
    Profile* profile,
    const webapps::AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_MAC)
  base::FilePath app_folder_shortcut_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  CHECK(base::PathExists(app_folder_shortcut_path));
  return base::DeletePathRecursively(app_folder_shortcut_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  LOG(INFO) << desktop_shortcut_path;
  CHECK(base::PathExists(desktop_shortcut_path));
  return base::DeleteFile(desktop_shortcut_path);
#else
  NOTREACHED() << "Not implemented on ChromeOS";
#endif
}

#if BUILDFLAG(IS_MAC)
bool OsIntegrationTestOverrideImpl::DeleteChromeAppsDir() {
  if (chrome_apps_folder_.IsValid()) {
    bool success = chrome_apps_folder_.Delete();
    if (!success) {
      // Creating shortcuts kicks of an asynchronous task to eventually update
      // the icon of `chrome_apps_folder_`. If that task happens to run during
      // the above Delete() call deletion might fail. If that is the case, a
      // single retry should be enough to be able to delete the folder anyway.
      success = chrome_apps_folder_.Delete();
    }
    return success;
  } else {
    return false;
  }
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
bool OsIntegrationTestOverrideImpl::DeleteDesktopDirOnLinux() {
  if (desktop_.IsValid()) {
    return desktop_.Delete();
  } else {
    return false;
  }
}
#endif  // BUILDFLAG(IS_LINUX)

bool OsIntegrationTestOverrideImpl::IsRunOnOsLoginEnabled(
    Profile* profile,
    const webapps::AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_LINUX)
  std::string shortcut_filename =
      "chrome-" + app_id + "-" + profile->GetBaseName().value() + ".desktop";
  base::i18n::ReplaceIllegalCharactersInPath(&shortcut_filename, '_');
  base::ReplaceChars(shortcut_filename, " ", "_", &shortcut_filename);
  return base::PathExists(startup().Append(shortcut_filename));
#elif BUILDFLAG(IS_MAC)
  std::string shortcut_filename = app_name + ".app";
  base::FilePath app_shortcut_path =
      chrome_apps_folder().Append(shortcut_filename);
  return startup_enabled_[app_shortcut_path];
#else
  NOTREACHED() << "Not implemented on ChromeOS";
#endif
}

bool OsIntegrationTestOverrideImpl::IsFileExtensionHandled(
    Profile* profile,
    const webapps::AppId& app_id,
    std::string app_name,
    std::string file_extension) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  bool is_file_handled = false;
#if BUILDFLAG(IS_MAC)
  const base::FilePath test_file_path =
      chrome_apps_folder().AppendASCII("test" + file_extension);
  const base::File test_file(
      test_file_path, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  const GURL test_file_url = net::FilePathToFileURL(test_file_path);
  base::FilePath app_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  is_file_handled =
      shell_integration::CanApplicationHandleURL(app_path, test_file_url);
  base::DeleteFile(test_file_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath user_applications_dir = applications();
  bool database_update_called = false;
  for (const LinuxFileRegistration& command : linux_file_registration_) {
    if (command.xdg_command.contains(app_id) &&
        command.xdg_command.contains(profile->GetPath().BaseName().value())) {
      if (base::StartsWith(command.xdg_command, "xdg-mime install")) {
        is_file_handled =
            command.file_contents.contains("\"*" + file_extension + "\"");
      } else {
        CHECK(base::StartsWith(command.xdg_command, "xdg-mime uninstall"))
            << command.xdg_command;
        is_file_handled = false;
      }
    }

    // Verify if the mimeinfo.cache is also updated. See
    // web_app_file_handler_registration_linux.cc for more information.
    if (base::StartsWith(command.xdg_command, "update-desktop-database")) {
      database_update_called =
          command.xdg_command.contains(user_applications_dir.value());
    }
  }
  is_file_handled = is_file_handled && database_update_called;
#endif
  return is_file_handled;
}

std::optional<SkBitmap> OsIntegrationTestOverrideImpl::GetShortcutIcon(
    Profile* profile,
    std::optional<base::FilePath> shortcut_dir,
    const webapps::AppId& app_id,
    const std::string& app_name,
    SquareSizePx suggested_size_px) {
#if BUILDFLAG(IS_MAC)
  if (!shortcut_dir.has_value()) {
#if BUILDFLAG(IS_MAC)
    shortcut_dir = chrome_apps_folder();
#endif
  }
  CHECK(!shortcut_dir->empty());
  base::FilePath shortcut_path =
      GetShortcutPath(profile, *shortcut_dir, app_id, app_name);
  if (!base::PathExists(shortcut_path)) {
    return std::nullopt;
  }
  return GetIconFromShortcutFile(shortcut_path);
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  WebAppProvider* provider = WebAppProvider::GetForLocalAppsUnchecked(profile);
  if (!provider) {
    return std::nullopt;
  }
  return IconManagerReadIconForSize(provider->icon_manager(), app_id,
                                    suggested_size_px);
#else
  NOTREACHED() << "Not implemented";
#endif
}

std::optional<SkColor>
OsIntegrationTestOverrideImpl::GetShortcutIconTopLeftColor(
    Profile* profile,
    base::FilePath shortcut_dir,
    const webapps::AppId& app_id,
    const std::string& app_name,
    SquareSizePx size_px) {
  std::optional<SkBitmap> bitmap = GetShortcutIcon(
      profile,
      shortcut_dir.empty() ? std::nullopt : std::optional(shortcut_dir), app_id,
      app_name, size_px);
  if (!bitmap) {
    return std::nullopt;
  }
  return bitmap->getColor(0, 0);
}

base::FilePath OsIntegrationTestOverrideImpl::GetShortcutPath(
    Profile* profile,
    base::FilePath shortcut_dir,
    const webapps::AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_MAC)
  base::ScopedAllowBlockingForTesting allow_blocking;
  AppShimRegistry* registry = AppShimRegistry::Get();
  std::set<base::FilePath> app_installed_profiles =
      registry->GetInstalledProfilesForApp(app_id);
  if (app_installed_profiles.find(profile->GetPath()) ==
      app_installed_profiles.end()) {
    return base::FilePath();
  }

  std::string bundle_id = GetBundleIdentifierForShim(app_id);
  auto bundles = BundleInfoPlist::SearchForBundlesById(bundle_id, shortcut_dir);
  // `SearchForBundlesById` can find bundles in multiple locations. For this
  // test, only the bundle in the given `shortcut_dir` is desired.
  for (const auto& bundle : bundles) {
    if (bundle.bundle_path().DirName() == shortcut_dir) {
      return bundle.bundle_path();
    }
  }
#elif BUILDFLAG(IS_LINUX)
  std::string shortcut_filename =
      "chrome-" + app_id + "-" + profile->GetBaseName().value() + ".desktop";
  base::i18n::ReplaceIllegalCharactersInPath(&shortcut_filename, '_');
  base::ReplaceChars(shortcut_filename, " ", "_", &shortcut_filename);
  base::FilePath shortcut_path = shortcut_dir.Append(shortcut_filename);
  if (base::PathExists(shortcut_path)) {
    return shortcut_path;
  }
#endif
  return base::FilePath();
}

bool OsIntegrationTestOverrideImpl::IsShortcutCreated(
    Profile* profile,
    const webapps::AppId& app_id,
    const std::string& app_name) {
#if BUILDFLAG(IS_MAC)
  base::FilePath app_shortcut_path =
      GetShortcutPath(profile, chrome_apps_folder(), app_id, app_name);
  return base::PathExists(app_shortcut_path);
#elif BUILDFLAG(IS_LINUX)
  base::FilePath desktop_shortcut_path =
      GetShortcutPath(profile, desktop(), app_id, app_name);
  return base::PathExists(desktop_shortcut_path);
#else
  NOTREACHED() << "Not implemented on ChromeOS";
#endif
}

bool OsIntegrationTestOverrideImpl::IsAppPinnedToTaskbar(
    const webapps::AppId& app_id) const {
  return taskbar_pinned_apps_.contains(app_id);
}

bool OsIntegrationTestOverrideImpl::HasOsIntegrationResourcesDirectory(
    Profile* profile,
    const webapps::AppId& app_id) {
  return base::PathExists(GetOsIntegrationResourcesDirectoryForApp(
      profile->GetPath(), app_id, GURL()));
}

bool OsIntegrationTestOverrideImpl::AreShortcutsMenuRegistered() {
  return !shortcut_menu_apps_registered_.empty();
}

base::expected<bool, std::string>
OsIntegrationTestOverrideImpl::IsUninstallRegisteredWithOs(
    const webapps::AppId& app_id,
    const std::string& app_name,
    Profile* profile) {
  return base::unexpected("Uninstall registration not supported.");
}

const OsIntegrationTestOverrideImpl::AppProtocolList&
OsIntegrationTestOverrideImpl::protocol_scheme_registrations() {
  return protocol_scheme_registrations_;
}

OsIntegrationTestOverrideImpl*
OsIntegrationTestOverrideImpl::AsOsIntegrationTestOverrideImpl() {
  return this;
}

#if BUILDFLAG(IS_MAC)
bool OsIntegrationTestOverrideImpl::IsChromeAppsValid() {
  return chrome_apps_folder_.IsValid();
}
base::FilePath OsIntegrationTestOverrideImpl::chrome_apps_folder() {
  return chrome_apps_folder_.GetPath();
}
void OsIntegrationTestOverrideImpl::EnableOrDisablePathOnLogin(
    const base::FilePath& file_path,
    bool enable_on_login) {
  startup_enabled_[file_path] = enable_on_login;
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX)
base::FilePath OsIntegrationTestOverrideImpl::desktop() {
  return desktop_.GetPath();
}
base::FilePath OsIntegrationTestOverrideImpl::startup() {
  return xdg_config_home_dir_.GetPath().Append("autostart");
}
base::FilePath OsIntegrationTestOverrideImpl::applications() {
  return xdg_data_home_dir_.GetPath().Append("applications");
}
base::FilePath OsIntegrationTestOverrideImpl::xdg_data_home_dir() {
  return xdg_data_home_dir_.GetPath();
}
base::Environment* OsIntegrationTestOverrideImpl::environment() {
  return &environment_;
}
#endif  // BUILDFLAG(IS_LINUX)

void OsIntegrationTestOverrideImpl::RegisterProtocolSchemes(
    const webapps::AppId& app_id,
    std::vector<std::string> protocols) {
  protocol_scheme_registrations_.emplace_back(app_id, std::move(protocols));
}

OsIntegrationTestOverrideImpl::OsIntegrationTestOverrideImpl(
    const base::FilePath& base_path) {
  // Initialize all directories used. The success & the CHECK are separated to
  // ensure that these function calls occur on release builds.

  bool success;
  if (base_path.empty()) {
    success = outer_temp_dir_.CreateUniqueTempDir();
  } else {
    success = outer_temp_dir_.CreateUniqueTempDirUnderPath(base_path);
  }
  CHECK(success);
#if BUILDFLAG(IS_MAC)
  success = chrome_apps_folder_.CreateUniqueTempDirUnderPath(
      outer_temp_dir_.GetPath());
  CHECK(success);
#elif BUILDFLAG(IS_LINUX)
  success = desktop_.CreateUniqueTempDirUnderPath(outer_temp_dir_.GetPath());
  CHECK(success);
  success = startup_.CreateUniqueTempDirUnderPath(outer_temp_dir_.GetPath());
  CHECK(success);
  success = xdg_config_home_dir_.CreateUniqueTempDirUnderPath(
      outer_temp_dir_.GetPath());
  CHECK(success);
  success = xdg_data_home_dir_.CreateUniqueTempDirUnderPath(
      outer_temp_dir_.GetPath());
  CHECK(success);
#endif

#if BUILDFLAG(IS_LINUX)
  auto callback = base::BindRepeating([](base::FilePath filename_in,
                                         std::string xdg_command,
                                         std::string file_contents) {
    auto test_override = OsIntegrationTestOverrideImpl::Get();
    CHECK(test_override);
    LinuxFileRegistration file_registration = LinuxFileRegistration();
    file_registration.file_name = filename_in;
    file_registration.xdg_command = xdg_command;
    file_registration.file_contents = file_contents;
    test_override->linux_file_registration_.push_back(file_registration);
    return true;
  });
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(std::move(callback));
  user_desktop_override_ = std::make_unique<base::ScopedPathOverride>(
      base::DIR_USER_DESKTOP, desktop_.GetPath());
  base::FilePath applications_path =
      xdg_data_home_dir_.GetPath().AppendASCII("applications");
  CHECK(base::CreateDirectory(applications_path))
      << "could not create applications directory.";
  base::FilePath autostart_path =
      xdg_config_home_dir_.GetPath().AppendASCII("autostart");
  CHECK(base::CreateDirectory(autostart_path))
      << "could not create applications directory.";
  environment_.Set("XDG_DATA_HOME", xdg_data_home_dir_.GetPath().value());
  environment_.Set(base::nix::kXdgConfigHomeEnvVar,
                   xdg_config_home_dir_.GetPath().value());
#endif

}

OsIntegrationTestOverrideImpl::~OsIntegrationTestOverrideImpl() {
  // Perform any cleanup necessary to clean OS integration state that isn't
  // already handled by the destruction of the member variables.

  // Sometimes the test deletes the directory manually - so use !IsValid() to
  // allow this to occur without causing an issue.
#if BUILDFLAG(IS_MAC)
  EXPECT_TRUE(!chrome_apps_folder_.IsValid() || DeleteChromeAppsDir());
#elif BUILDFLAG(IS_LINUX)
  EXPECT_TRUE(!desktop_.IsValid() || desktop_.Delete());
  EXPECT_TRUE(!startup_.IsValid() || startup_.Delete());
  EXPECT_TRUE(!xdg_data_home_dir_.IsValid() || xdg_data_home_dir_.Delete());
  EXPECT_TRUE(!xdg_config_home_dir_.IsValid() || xdg_config_home_dir_.Delete());
  // Reset the file handling callback.
  SetUpdateMimeInfoDatabaseOnLinuxCallbackForTesting(base::NullCallback());
#endif
}

}  // namespace web_app
