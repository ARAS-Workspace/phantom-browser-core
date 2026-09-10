// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_util.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/external_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/browser/ui_util.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

namespace {

}  // namespace

class ExtensionUtilUnittest : public ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }
};

TEST_F(ExtensionUtilUnittest, SetAllowFileAccess) {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 3,
           "host_permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Calling SetAllowFileAccess should reload the extension with file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Removing the file access should reload the extension again back to not
  // having file access.
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }

  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(ExtensionUtilUnittest, SetAllowFileAccessWhileDisabled) {
  constexpr char kManifest[] =
      R"({
           "name": "foo",
           "version": "1.0",
           "manifest_version": 3,
           "host_permissions": ["<all_urls>"]
         })";

  TestExtensionDir dir;
  dir.WriteManifest(kManifest);

  ChromeTestExtensionLoader loader(profile());
  // An unpacked extension would get file access by default, so disabled it on
  // the loader.
  loader.set_allow_file_access(false);

  scoped_refptr<const Extension> extension =
      loader.LoadExtension(dir.UnpackedPath());
  const std::string extension_id = extension->id();

  GURL file_url("file://etc");
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  int tab_id = sessions::SessionTabHelper::IdForTab(web_contents.get()).id();

  // Initially the file access pref will be false and the extension will not be
  // able to capture a file URL page.
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension then calling SetAllowFileAccess should reload the
  // extension with file access.
  registrar()->DisableExtension(extension_id,
                                {disable_reason::DISABLE_USER_ACTION});
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), true);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(registrar()->IsExtensionEnabled(extension_id));

  registrar()->EnableExtension(extension_id);
  EXPECT_TRUE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_TRUE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));

  // Disabling the extension and then removing the file access should reload it
  // again back to not having file access. Regression test for
  // crbug.com/40061772.
  registrar()->DisableExtension(extension_id,
                                {disable_reason::DISABLE_USER_ACTION});
  {
    TestExtensionRegistryObserver observer(registry(), extension_id);
    util::SetAllowFileAccess(extension_id, browser_context(), false);
    extension = observer.WaitForExtensionInstalled();
  }
  // The extension should still be disabled.
  EXPECT_FALSE(registrar()->IsExtensionEnabled(extension_id));

  registrar()->EnableExtension(extension_id);
  EXPECT_FALSE(util::AllowFileAccess(extension_id, profile()));
  EXPECT_FALSE(extension->permissions_data()->CanCaptureVisiblePage(
      file_url, tab_id, nullptr, CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(ExtensionUtilUnittest, HasIsolatedStorage) {
  // Platform apps should have isolated storage.
  scoped_refptr<const Extension> app =
      ExtensionBuilder("foo_app", ExtensionBuilder::Type::PLATFORM_APP).Build();
  EXPECT_TRUE(app->is_platform_app());
  EXPECT_TRUE(util::HasIsolatedStorage(*app.get(), profile()));

  // Extensions should not have isolated storage.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("foo_ext").Build();
  EXPECT_FALSE(extension->is_platform_app());
  EXPECT_FALSE(util::HasIsolatedStorage(*extension.get(), profile()));
}

TEST_F(ExtensionUtilUnittest, FixupLongExtensionName) {
  const std::string long_extension_name =
      "A very long extension name etc A very long extension name etc A very "
      "long extension name etc A very long extension name etc";
  std::u16string expected_fixup_extension_name =
      u"A very long extension name etc A very long extension name etc A very "
      u"long\u2026";

  std::u16string fixup_extension_name =
      ui_util::GetFixupExtensionNameForUIDisplay(long_extension_name);
  EXPECT_EQ(fixup_extension_name, expected_fixup_extension_name);
}

using ExtensionUtilDeathTest = testing::Test;

TEST_F(ExtensionUtilDeathTest, GetCWSWritingReviewUrl_InvalidId) {
  const ExtensionId kInvalidId = "invalid_id_format";

  for (const char* invalid_id : {kInvalidId.c_str(), "", "../../etc/passwd"}) {
    EXPECT_CHECK_DEATH(util::GetCWSWritingReviewUrl(
        invalid_id, util::CWSReviewSource::kExtensionsMenu));
  }
}

TEST_F(ExtensionUtilUnittest, GetCWSWritingReviewUrl) {
  const ExtensionId kValidId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  EXPECT_EQ(
      util::GetCWSWritingReviewUrl(
          kValidId, util::CWSReviewSource::kExtensionsMenu)
          .spec(),
      base::StringPrintf("https://chromewebstore.google.com/detail/"
                         "%s/reviews?action=write&source=extensions_menu",
                         kValidId.c_str()));

  EXPECT_EQ(
      util::GetCWSWritingReviewUrl(
          kValidId, util::CWSReviewSource::kExtensionsPage)
          .spec(),
      base::StringPrintf("https://chromewebstore.google.com/detail/"
                         "%s/reviews?action=write&source=extensions_page",
                         kValidId.c_str()));

  EXPECT_EQ(
      util::GetCWSWritingReviewUrl(
          kValidId, util::CWSReviewSource::kContextMenu)
          .spec(),
      base::StringPrintf("https://chromewebstore.google.com/detail/"
                         "%s/reviews?action=write&source=context_menu",
                         kValidId.c_str()));
}

}  // namespace extensions
