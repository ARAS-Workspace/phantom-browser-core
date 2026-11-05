// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/updater/extension_updater.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/fake_crx_installer.h"
#include "chrome/browser/extensions/mock_crx_installer.h"
#include "chrome/browser/extensions/sync/extension_sync_data.h"
#include "chrome/browser/extensions/test_extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/updater/chrome_extension_downloader_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/crx_file/id_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/update_client/update_query_params.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/blocklist_extension_prefs.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/browser/crx_installer.h"
#include "extensions/browser/delayed_install_manager.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pending_extension_manager.h"
#include "extensions/browser/pref_names.h"
#include "extensions/browser/test_extension_prefs.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/extension_downloader_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "extensions/browser/updater/extension_downloader_test_helper.h"
#include "extensions/browser/updater/extension_downloader_types.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/browser/updater/request_queue_impl.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/manifest_url_handlers.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "extensions/common/verifier_formats.h"
#include "net/base/backoff_entry.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;
using extensions::mojom::ManifestLocation;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;
using update_client::UpdateQueryParams;

namespace extensions {

using Error = ExtensionDownloaderDelegate::Error;
using PingResult = ExtensionDownloaderDelegate::PingResult;

namespace {

const net::BackoffEntry::Policy kNoBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  1000,

  // Initial delay for exponential back-off in ms.
  0,

  // Factor by which the waiting time will be multiplied.
  0,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  0,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

constexpr int kExpectedLoadFlags = net::LOAD_DISABLE_CACHE;

constexpr int kExpectedLoadFlagsForDownloadWithCookies =
    net::LOAD_DISABLE_CACHE;

// An update server other than the store.
constexpr char kThirdPartyUpdateUrl[] = "http://localhost/test/updates.xml";

// Manifest key used to make multiple extensions with the same ID.
constexpr char kExtensionManifestKey[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDowC9B4+K6zbl4PnALNyOUgra/"
    "MPdD8gZ39Fk/IAJWt03qrN7vz1gd/"
    "mmrBg0EEIsyLRmUmfyVEfvcIUOZxFqn4A9D2aaRSvNHy9qkasZMBDEql8Nt2iNZm/"
    "kGS7sizidDV6Bc/vyLNiH1gKOXBQ42JIxKjgtrmnhGV2giw2vJGwIDAQAB";

constexpr base::TimeDelta kUpdateFrequency = base::Seconds(15);

// A minimal stub implementation of the ExtensionRegistrar::Delegate.
class StubExtensionRegistrarDelegate : public ExtensionRegistrar::Delegate {
 public:
  StubExtensionRegistrarDelegate() = default;
  ~StubExtensionRegistrarDelegate() override = default;

  // ExtensionRegistrar::Delegate:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override {}
  void PostActivateExtension(
      scoped_refptr<const Extension> extension) override {}
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override {}
  void PostUninstallExtension(scoped_refptr<const Extension> extension,
                              base::OnceClosure done_callback) override {}
  void ShowExtensionDisabledError(const Extension* extension,
                                  bool is_remote_install) override {}
  void LoadExtensionForReload(const ExtensionId& extension_id,
                              const base::FilePath& path) override {}
  void LoadExtensionForReloadWithQuietFailure(
      const ExtensionId& extension_id,
      const base::FilePath& path) override {}
  bool CanEnableExtension(const Extension* extension) override { return true; }
  bool CanDisableExtension(const Extension* extension) override { return true; }
  void GrantActivePermissions(const Extension* extension) override {}
  void UpdateExternalExtensionAlert() override {}
  void OnExtensionInstalled(const Extension* extension,
                            const syncer::StringOrdinal& page_ordinal,
                            int install_flags,
                            base::DictValue ruleset_install_prefs) override {}
};

}  // namespace

class TestCrxInstallerFactory
    : public ExtensionUpdater::CrxInstallerFactoryForTest {
 public:
  TestCrxInstallerFactory() = default;
  TestCrxInstallerFactory(const TestCrxInstallerFactory&) = delete;
  TestCrxInstallerFactory& operator=(const TestCrxInstallerFactory&) = delete;
  ~TestCrxInstallerFactory() override = default;

  const ExtensionId& crx_extension_id() const { return crx_extension_id_; }

  const base::FilePath& crx_install_path() const { return crx_install_path_; }

  // Add a fake crx installer to be returned by a call to
  // CreateUpdateInstaller() with a specific ID.
  void AddFakeCrxInstaller(const std::string& id,
                           scoped_refptr<CrxInstaller> crx_installer) {
    fake_crx_installers_[id] = crx_installer;
  }

  // ExtensionUpdaterDelegate:
  scoped_refptr<CrxInstaller> CreateUpdateInstaller(
      const CRXFileInfo& file,
      bool file_ownership_passed) override {
    crx_extension_id_ = file.extension_id;
    crx_install_path_ = file.path;

    if (fake_crx_installers_.contains(crx_extension_id_)) {
      return fake_crx_installers_[crx_extension_id_];
    }

    return nullptr;
  }

 private:
  // Hold the set of ids that CreateUpdateInstaller() returns.
  std::map<std::string, scoped_refptr<CrxInstaller>> fake_crx_installers_;

  // The last values from the CRXFileInfo passed to CreateUpdateInstaller().
  ExtensionId crx_extension_id_;
  base::FilePath crx_install_path_;
};

// Helper to create ExtensionDownloader::Factory instances. This needs to be
// its own class, not part of ExtensionUpdaterTest, because individual tests
// create their own ExtensionDownloaderTestHelpers, and hence their own
// SharedURLLoaderFactory instances.
class TestDownloaderFactory {
 public:
  explicit TestDownloaderFactory(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : test_shared_url_loader_factory_(url_loader_factory) {}

  TestDownloaderFactory(const TestDownloaderFactory&) = delete;
  TestDownloaderFactory& operator=(const TestDownloaderFactory&) = delete;

  ~TestDownloaderFactory() = default;

  ExtensionDownloader::Factory GetDownloaderFactory() {
    return base::BindRepeating(
        &TestDownloaderFactory::CreateExtensionDownloader,
        base::Unretained(this));
  }

  void OverrideDownloaderDelegate(ExtensionDownloaderDelegate* delegate) {
    downloader_delegate_override_ = delegate;
  }

 private:
  std::unique_ptr<ExtensionDownloader> CreateExtensionDownloader(
      ExtensionDownloaderDelegate* delegate) {
    std::unique_ptr<ExtensionDownloader> downloader =
        ChromeExtensionDownloaderFactory::CreateForURLLoaderFactory(
            test_shared_url_loader_factory_,
            downloader_delegate_override_ ? downloader_delegate_override_.get()
                                          : delegate,
            GetTestVerifierFormat());
    return downloader;
  }

  raw_ptr<ExtensionDownloaderDelegate> downloader_delegate_override_ = nullptr;

  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
};

bool ShouldInstallExtensionsOnly(const Extension* extension,
                                 content::BrowserContext* context) {
  return extension->GetType() == Manifest::Type::kExtension;
}

bool ShouldInstallThemesOnly(const Extension* extension,
                             content::BrowserContext* context) {
  return extension->is_theme();
}

bool ShouldAlwaysInstall(const Extension* extension,
                         content::BrowserContext* context) {
  return true;
}

// Loads some pending extension records into a pending extension manager.
void SetupPendingExtensionManagerForTest(
    int count,
    const GURL& update_url,
    PendingExtensionManager* pending_extension_manager) {
  for (int i = 1; i <= count; ++i) {
    PendingExtensionInfo::ShouldAllowInstallPredicate should_allow_install =
        (i % 2 == 0) ? &ShouldInstallThemesOnly : &ShouldInstallExtensionsOnly;
    const bool kIsFromSync = true;
    const bool kMarkAcknowledged = false;
    const bool kRemoteInstall = false;
    std::string id =
        crx_file::id_util::GenerateId(base::StringPrintf("extension%i", i));

    pending_extension_manager->AddForTesting(PendingExtensionInfo(
        id, std::string(), update_url, base::Version(), should_allow_install,
        kIsFromSync, ManifestLocation::kInternal, Extension::NO_FLAGS,
        kMarkAcknowledged, kRemoteInstall));
  }
}

// Takes a string with KEY=VALUE parameters separated by '&' in |params| and
// puts the key/value pairs into |result|. For keys with no value, the empty
// string is used. So for "a=1&b=foo&c", result would map "a" to "1", "b" to
// "foo", and "c" to "".
static void ExtractParameters(const std::string& params,
                              std::map<std::string, std::string>* result) {
  for (const std::string& pair : base::SplitString(
           params, "&", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    std::vector<std::string> key_val = base::SplitString(
        pair, "=", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (!key_val.empty()) {
      std::string key = key_val[0];
      EXPECT_TRUE(result->find(key) == result->end());
      (*result)[key] = (key_val.size() == 2) ? key_val[1] : std::string();
    } else {
      NOTREACHED();
    }
  }
}

static void VerifyQueryAndExtractParameters(
    const std::string& query,
    std::map<std::string, std::string>* result) {
  std::map<std::string, std::string> params;
  ExtractParameters(query, &params);

  std::string omaha_params = UpdateQueryParams::Get(UpdateQueryParams::CRX);
  std::map<std::string, std::string> expected;
  ExtractParameters(omaha_params, &expected);

  for (auto it = expected.begin(); it != expected.end(); ++it) {
    EXPECT_EQ(it->second, params[it->first]);
  }

  EXPECT_EQ(1U, params.count("x"));
  std::string decoded = base::UnescapeBinaryURLComponent(params["x"]);
  ExtractParameters(decoded, result);
}

// All of our tests that need to use private APIs of ExtensionUpdater live
// inside this class (which is a friend to ExtensionUpdater).
class ExtensionUpdaterTest : public testing::Test {
 public:
  ExtensionUpdaterTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    prefs_ = std::make_unique<TestExtensionPrefs>(
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        std::make_unique<TestingProfile>());
    // The registrar needs a delegate in order to call certain methods on it.
    ExtensionRegistrar::Get(prefs_->browser_context())
        ->Init(&stub_extension_registrar_delegate_,
               /*extensions_enabled=*/true,
               base::CommandLine::ForCurrentProcess(), base::FilePath(),
               base::FilePath());
  }

  void TearDown() override {
    // Some tests create URLRequestContextGetters, whose destruction must run
    // on the IO thread. Make sure the IO loop spins before shutdown so that
    // those objects are released.
    RunUntilIdle();
    // Reset the ExtensionRegistrar delegate.
    ExtensionRegistrar::Get(prefs_->browser_context())
        ->Init(/*delegate=*/nullptr, /*extensions_enabled=*/true,
               base::CommandLine::ForCurrentProcess(), base::FilePath(),
               base::FilePath());
    prefs_.reset();
  }

  Profile* profile() {
    return Profile::FromBrowserContext(prefs_->browser_context());
  }

  ExtensionPrefs* extension_prefs() { return prefs_->prefs(); }

  PrefService* pref_service() { return prefs_->pref_service(); }

  // Creates test extensions and inserts them into list. The name and
  // version are all based on their index. If `update_url` is non-null, it
  // will be used as the update_url for each extension. If `key` is non-null,
  // it will be used as manifest_keys::kKey to force an extension to have a
  // particular ID. `key` is only valid if `count` is 1.
  // The `id` is used to distinguish extension names and make sure that
  // no two extensions share the same name.
  void CreateTestExtensions(int id,
                            int count,
                            ExtensionList* list,
                            const std::string* update_url,
                            ManifestLocation location,
                            std::optional<std::string> key = std::nullopt) {
    if (key.has_value()) {
      CHECK_EQ(count, 1) << "Can't create two extensions with the same key";
    }
    for (int i = 1; i <= count; i++) {
      base::DictValue manifest;
      manifest.Set(manifest_keys::kVersion, base::StringPrintf("%d.0.0.0", i));
      manifest.Set(manifest_keys::kName,
                   base::StringPrintf("Extension %d.%d", id, i));
      manifest.Set(manifest_keys::kManifestVersion, 2);
      if (update_url) {
        manifest.Set(manifest_keys::kUpdateURL, *update_url);
      }
      if (key.has_value()) {
        manifest.Set(manifest_keys::kKey, *key);
      }
      scoped_refptr<Extension> e =
          prefs_->AddExtensionWithManifest(manifest, location);
      ASSERT_TRUE(e.get() != nullptr);
      list->push_back(e);
    }
  }

  void SetExtensions(
      const ExtensionList& extensions,
      const ExtensionList& disabled_extensions,
      const ExtensionList& blocklisted_extensions = ExtensionList()) {
    auto* registry = ExtensionRegistry::Get(profile());
    registry->ClearAll();
    for (const auto& extension : extensions) {
      registry->AddEnabled(extension);
    }
    for (const auto& extension : disabled_extensions) {
      registry->AddDisabled(extension);
    }
    for (const auto& extension : blocklisted_extensions) {
      registry->AddBlocklisted(extension);
    }
  }

  void RunUntilIdle() {
    prefs_->pref_service()->CommitPendingWrite();
    base::RunLoop().RunUntilIdle();
  }

  void SimulateTimerFired(ExtensionUpdater* updater) { updater->NextCheck(); }

  // Adds a Result with the given data to results.
  void AddParseResult(const std::string& id,
                      const std::string& version,
                      const std::string& url,
                      UpdateManifestResults* results) {
    UpdateManifestResult result;
    result.extension_id = id;
    result.version = version;
    result.crx_url = GURL(url);
    results->update_list.push_back(result);
  }

  void StartUpdateCheck(ExtensionDownloader* downloader,
                        ManifestFetchData* fetch_data) {
    downloader->StartUpdateCheck(
        std::unique_ptr<ManifestFetchData>(fetch_data));
  }

  size_t ManifestFetchersCount(ExtensionDownloader* downloader) {
    return downloader->manifests_queue_.size() +
           (downloader->HasActiveManifestRequestForTesting() ? 1 : 0);
  }

  // Returns the ids of the manifest fetch that `updater` is running.
  ExtensionIdSet GetActiveManifestFetchIds(ExtensionUpdater* updater) {
    const ManifestFetchData* fetch =
        updater->downloader_->manifests_queue_.active_request();
    return fetch ? fetch->GetExtensionIds() : ExtensionIdSet();
  }

  bool GetExtensionExistingVersion(ExtensionUpdater* updater,
                                   const ExtensionId& id,
                                   std::string* version) {
    return updater->GetExtensionExistingVersion(id, version);
  }

  // Runs a full update check over what the test has set up. Returns the ids of
  // the one manifest fetch it started; with no ids to fetch, the check has to
  // finish right away.
  ExtensionIdSet GetIdsOfFullUpdateCheck() {
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    updater.Start();
    updater.CheckNow(ExtensionUpdater::CheckParams());
    ExtensionIdSet ids = GetActiveManifestFetchIds(&updater);
    EXPECT_EQ(!ids.empty(), updater.HasFullCheckInProgress());
    EXPECT_EQ(ids.empty() ? 0u : 1u,
              ManifestFetchersCount(updater.downloader_.get()));
    return ids;
  }

  std::set<std::string> GetRunningInstallIds(const ExtensionUpdater& updater) {
    std::set<std::string> ret;
    for (const auto& pair : updater.running_crx_installs_)
      ret.insert(pair.second.info.extension_id);
    return ret;
  }

  const DownloadFailure* GetFailureWithId(
      const std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>>&
          failures,
      const ExtensionId& id) {
    auto it = std::ranges::find(
        failures, id, [](const auto& failure) { return failure.first.id; });
    return it == failures.end() ? nullptr : &it->second;
  }

  void TestExtensionUpdateCheckRequests(bool pending) {
    // Create an extension with an update_url.
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    std::string update_url("http://foo.com/bar");
    ExtensionList extensions;
    PendingExtensionManager* pending_extension_manager =
        PendingExtensionManager::Get(profile());
    if (pending) {
      SetupPendingExtensionManagerForTest(1, GURL(update_url),
                                          pending_extension_manager);
    } else {
      CreateTestExtensions(1, 1, &extensions, &update_url,
                           ManifestLocation::kInternal);
      SetExtensions(extensions, ExtensionList());
    }

    // Set up and start the updater.
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), base::Days(1),
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    updater.Start();

    // Tell the update that it's time to do update checks.
    SimulateTimerFired(&updater);

    // Get the url our loader was asked to fetch.
    const ManifestFetchData& fetch =
        *updater.downloader_->manifests_queue_.active_request();

    const GURL& url = fetch.full_url();

    EXPECT_FALSE(url.is_empty());
    EXPECT_TRUE(url.is_valid());
    EXPECT_TRUE(url.SchemeIs("http"));
    EXPECT_EQ("foo.com", url.GetHost());
    EXPECT_EQ("/bar", url.GetPath());

    // Validate the extension request parameters in the query. It should
    // look something like "x=id%3D<id>%26v%3D<version>%26uc".
    EXPECT_TRUE(url.has_query());
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(url.GetQuery(), &params);
    if (pending) {
      EXPECT_TRUE(pending_extension_manager->IsIdPending(params["id"]));
      EXPECT_EQ("0.0.0.0", params["v"]);
    } else {
      EXPECT_EQ(extensions[0]->id(), params["id"]);
      EXPECT_EQ(extensions[0]->VersionString(), params["v"]);
    }
    EXPECT_EQ("", params["uc"]);
  }

  void TestUpdateUrlDataEmpty() {
    ExtensionDownloaderTestHelper helper;
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an empty update URL data string does not cause a ap=
    // option to appear in the x= parameter.
    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    AddExtensionToFetchDataForTesting(fetch_data.get(), id, version,
                                      kUpdateURL);

    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().GetQuery(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(0U, params.count("ap"));
  }

  void TestUpdateUrlDataSimple() {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an update URL data string causes an appropriate ap=
    // option to appear in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(id, version,
                             ExtensionDownloaderTestHelper::GetTestPingData(),
                             "bar", std::string(), ManifestLocation::kInternal,
                             DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().GetQuery(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ("bar", params["ap"]);
  }

  void TestUpdateUrlDataCompound() {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    // Make sure that an update URL data string causes an appropriate ap=
    // option to appear in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, ExtensionDownloaderTestHelper::GetTestPingData(),
        "a=1&b=2&c", std::string(), ManifestLocation::kInternal,
        DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().GetQuery(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ("a%3D1%26b%3D2%26c", params["ap"]);
  }

  void TestUpdateUrlDataFromUrl(const std::string& update_url,
                                DownloadFetchPriority fetch_priority,
                                int num_extensions) {
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionList extensions;

    CreateTestExtensions(1, num_extensions, &extensions, &update_url,
                         ManifestLocation::kInternal);

    for (int i = 0; i < num_extensions; ++i) {
      const std::string& id = extensions[i]->id();
      EXPECT_CALL(helper.delegate(), GetPingDataForExtension(id, _));

      helper.downloader().AddPendingExtension(ExtensionDownloaderTask(
          id, ManifestURL::GetUpdateURL(extensions[i].get()),
          extensions[i]->location(), false, 0, fetch_priority,
          extensions[i]->version(), extensions[i]->GetType(), std::string()));
    }

    // Get the headers our loader was asked to fetch.
    base::RunLoop loop;
    net::HttpRequestHeaders last_request_headers;
    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          last_request_headers = request.headers;
          loop.Quit();
        }));

    helper.downloader().StartAllPending(nullptr);
    EXPECT_TRUE(helper.downloader().HasActiveManifestRequestForTesting());

    loop.Run();

    // Make sure that no update URL data was sent.
    const ManifestFetchData& fetch =
        *helper.downloader().manifests_queue_.active_request();
    const std::string& fetcher_url = fetch.full_url().spec();
    std::string::size_type x = fetcher_url.find("x=");
    EXPECT_NE(std::string::npos, x);
    std::string::size_type ap = fetcher_url.find("ap%3D", x);
    EXPECT_EQ(std::string::npos, ap);

    net::HttpRequestHeaders fetch_headers;
    std::swap(fetch_headers, last_request_headers);
    EXPECT_FALSE(fetch_headers.HasHeader("X-Goog-Update-Interactivity"));
    EXPECT_FALSE(fetch_headers.HasHeader("X-Goog-Update-AppId"));
    EXPECT_FALSE(fetch_headers.HasHeader("X-Goog-Update-Updater"));
  }

  void TestInstallSource() {
    const std::string id(32, 'a');
    const std::string version = "1.0";
    const std::string install_source = "instally";

    // Make sure that an installsource= appears in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, ExtensionDownloaderTestHelper::GetTestPingData(),
        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData, install_source,
        ManifestLocation::kInternal, DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().GetQuery(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(install_source, params["installsource"]);
  }

  void TestInstallLocation() {
    const std::string id(32, 'a');
    const std::string version = "1.0";
    const std::string install_location = "external";

    // Make sure that installedby= appears in the x= parameter.
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo")));
    fetch_data->AddExtension(
        id, version, ExtensionDownloaderTestHelper::GetTestPingData(),
        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData, std::string(),
        ManifestLocation::kExternalPrefDownload,
        DownloadFetchPriority::kBackground);
    std::map<std::string, std::string> params;
    VerifyQueryAndExtractParameters(fetch_data->full_url().GetQuery(), &params);
    EXPECT_EQ(id, params["id"]);
    EXPECT_EQ(version, params["v"]);
    EXPECT_EQ(install_location, params["installedby"]);
  }

  void TestDetermineUpdates() {
    ExtensionDownloaderTestHelper helper;

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));

    // Check passing an empty list of parse results to DetermineUpdates
    UpdateManifestResults updates;
    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    EXPECT_TRUE(updateable.empty());
    EXPECT_TRUE(failures.empty());

    // Create two updates - expect that DetermineUpdates will return the first
    // one (v1.0 installed, v1.1 available) but not the second one (both
    // installed and available at v2.0).
    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.0.0.0",
                                      kUpdateURL);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "2.0.0.0",
                                      kUpdateURL);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    EXPECT_CALL(helper.delegate(), IsExtensionPending(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(helper.delegate(), GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.0.0.0"), Return(true)));
    EXPECT_CALL(helper.delegate(), GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("2.0.0.0"), Return(true)));

    updateable.clear();
    failures.clear();
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    ASSERT_EQ(1u, failures.size());
    EXPECT_EQ(id2, failures[0].first.id);
    EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
              failures[0].second.error);
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0].second->version);
  }

  void TestDetermineUpdatesError() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    UpdateManifestResults updates;

    // id1 => updatable (current version (1.1) is older than update version).
    // id2 => non_updateable (current version (2.0.0.0) is the same as update
    //        version).
    // id3 => non_updateable (manifest update version is empty).
    // id4 => errors (|updates| doesn't contain id4).
    // id5 => errors (the extension is not currently installed).
    // id6 => errors (manifest update version is invalid).
    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    const std::string id3 = crx_file::id_util::GenerateId("3");
    const std::string id4 = crx_file::id_util::GenerateId("4");
    const std::string id5 = crx_file::id_util::GenerateId("5");
    const std::string id6 = crx_file::id_util::GenerateId("6");

    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.0.0.0",
                                      kUpdateURL);
    AddParseResult(id1, "1.1", "http://localhost/e1_1.1.crx", &updates);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "2.0.0.0",
                                      kUpdateURL);
    AddParseResult(id2, "2.0.0.0", "http://localhost/e2_2.0.crx", &updates);

    // Empty update version in manifest.
    AddExtensionToFetchDataForTesting(fetch_data.get(), id3, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id3, "", "http://localhost/e3_3.0.crx", &updates);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id4, "0.0.0.0",
                                      kUpdateURL);

    AddExtensionToFetchDataForTesting(fetch_data.get(), id5, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id5, "5.0.0.0", "http://localhost/e5_5.0.crx", &updates);

    // Invalid update version in manifest.
    AddExtensionToFetchDataForTesting(fetch_data.get(), id6, "0.0.0.0",
                                      kUpdateURL);
    AddParseResult(id6, "invalid_version", "http://localhost/e6_6.0.crx",
                   &updates);

    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("2.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id3, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id5, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(false)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id6, _))
        .WillOnce(DoAll(SetArgPointee<1>("0.0.0.0"), Return(true)));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    std::vector<ExtensionId> ids_not_updateable({id2, id3});
    for (const auto& id : ids_not_updateable) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                failure->error);
    }
    std::vector<ExtensionId> ids_with_error({id4, id5, id6});
    for (const auto& id : ids_with_error) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                failure->error);
    }
    EXPECT_EQ(5u, failures.size());
    ASSERT_EQ(1u, updateable.size());
    EXPECT_EQ("1.1", updateable[0].second->version);
  }

  void TestDetermineUpdatesPending() {
    // Create a set of test extensions
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    PendingExtensionManager* pending_extension_manager =
        PendingExtensionManager::Get(profile());
    SetupPendingExtensionManagerForTest(3, GURL(), pending_extension_manager);

    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    UpdateManifestResults updates;

    std::list<std::string> ids_for_update_check =
        pending_extension_manager->GetPendingIdsForUpdateCheck();

    for (const std::string& id : ids_for_update_check) {
      AddExtensionToFetchDataForTesting(fetch_data.get(), id, "1.0.0.0",
                                        kUpdateURL);
      AddParseResult(id, "1.1", "http://localhost/e1_1.1.crx", &updates);
    }

    // The delegate will tell the downloader that all the extensions are
    // pending.
    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(true));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    // All the apps should be updateable.
    EXPECT_EQ(3u, updateable.size());
    EXPECT_TRUE(failures.empty());
  }

  void TestDetermineUpdatesDuplicates() {
    base::HistogramTester histogram_tester;
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();

    const std::string id1 = crx_file::id_util::GenerateId("1");
    const std::string id2 = crx_file::id_util::GenerateId("2");
    const std::string id3 = crx_file::id_util::GenerateId("3");
    const std::string id4 = crx_file::id_util::GenerateId("4");
    const std::string id5 = crx_file::id_util::GenerateId("5");
    const std::string id6 = crx_file::id_util::GenerateId("6");
    const std::string id7 = crx_file::id_util::GenerateId("7");

    GURL kUpdateURL("http://localhost/foo");
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(kUpdateURL));
    AddExtensionToFetchDataForTesting(fetch_data.get(), id1, "1.1.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id2, "1.2.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id3, "1.3.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id4, "1.4.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id5, "1.5.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id6, "1.6.0.0",
                                      kUpdateURL);
    AddExtensionToFetchDataForTesting(fetch_data.get(), id7, "1.7.0.0",
                                      kUpdateURL);

    UpdateManifestResults updates;
    AddParseResult(id1, "1.1.0.0", "http://localhost/e1_1.1.crx", &updates);
    AddParseResult(id2, "1.2.0.a", "http://localhost/e2_1.1.crx", &updates);
    AddParseResult(id3, "1.3.1.0", "http://localhost/e3_1.1.crx", &updates);
    AddParseResult(id4, "1.4.0.0", "http://localhost/e4_1.1.crx", &updates);
    AddParseResult(id4, "1.4.0.a", "http://localhost/e4_1.1.crx", &updates);
    AddParseResult(id5, "1.5.0.a", "http://localhost/e5_1.1.crx", &updates);
    AddParseResult(id5, "1.5.0.b", "http://localhost/e5_1.1.crx", &updates);
    AddParseResult(id6, "1.6.0.a", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.0.0", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.1.0", "http://localhost/e6_1.1.crx", &updates);
    AddParseResult(id6, "1.6.2.0", "http://localhost/e6_1.1.crx", &updates);

    EXPECT_CALL(delegate, IsExtensionPending(_)).WillRepeatedly(Return(false));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id1, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.1.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id2, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.2.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id3, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.3.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id4, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.4.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id5, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.5.0.0"), Return(true)));
    EXPECT_CALL(delegate, GetExtensionExistingVersion(id6, _))
        .WillOnce(DoAll(SetArgPointee<1>("1.6.0.0"), Return(true)));

    std::vector<std::pair<ExtensionDownloaderTask, UpdateManifestResult*>>
        updateable;
    std::vector<std::pair<ExtensionDownloaderTask, DownloadFailure>> failures;
    helper.downloader().DetermineUpdates(fetch_data->TakeAssociatedTasks(),
                                         updates, &updateable, &failures);
    std::vector<ExtensionId> ids_not_updateable({id1, id4});
    for (const auto& id : ids_not_updateable) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
                failure->error);
    }
    std::vector<ExtensionId> ids_with_error({id2, id5, id7});
    for (const auto& id : ids_with_error) {
      const auto* failure = GetFailureWithId(failures, id);
      ASSERT_TRUE(failure);
      EXPECT_EQ(ExtensionDownloaderDelegate::Error::MANIFEST_INVALID,
                failure->error);
    }
    EXPECT_EQ(5u, failures.size());
    ASSERT_EQ(2u, updateable.size());
    EXPECT_EQ("1.3.1.0", updateable[0].second->version);
    EXPECT_EQ("1.6.1.0", updateable[1].second->version);
  }

  void TestMultipleManifestDownloading() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch1(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch2(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch3(
        CreateManifestFetchData(kUpdateUrl));
    std::unique_ptr<ManifestFetchData> fetch4(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zero_days(/*rollcall=*/0, /*active=*/0, /*enabled=*/true,
                               /*disable_reasons=*/{});
    AddExtensionToFetchDataForTesting(fetch1.get(), "1111", "1.0", kUpdateUrl,
                                      zero_days);
    AddExtensionToFetchDataForTesting(fetch2.get(), "2222", "2.0", kUpdateUrl,
                                      zero_days);
    AddExtensionToFetchDataForTesting(fetch3.get(), "3333", "3.0", kUpdateUrl,
                                      zero_days);
    AddExtensionToFetchDataForTesting(fetch4.get(), "4444", "4.0", kUpdateUrl,
                                      zero_days);

    // This will start the first fetcher and queue the others. The next in queue
    // is started as each fetcher receives its response. Note that the fetchers
    // don't necessarily run in the order that they are started from here.
    GURL fetch1_url = fetch1->full_url();
    GURL fetch2_url = fetch2->full_url();
    GURL fetch3_url = fetch3->full_url();
    GURL fetch4_url = fetch4->full_url();

    // fetch1_url
    {
      helper.StartUpdateCheck(std::move(fetch1));
      RunUntilIdle();
      helper.test_url_loader_factory().AddResponse(fetch1_url.spec(), "",
                                                   net::HTTP_BAD_REQUEST);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
              _, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch1_url = GURL();

      RunUntilIdle();
    }

    // fetch2_url
    {
      helper.StartUpdateCheck(std::move(fetch2));
      RunUntilIdle();
      const std::string kInvalidXml = "invalid xml";
      helper.test_url_loader_factory().AddResponse(fetch2_url.spec(),
                                                   kInvalidXml, net::HTTP_OK);
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "2222", ExtensionDownloaderDelegate::Error::MANIFEST_INVALID, _,
              _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch2_url = GURL();

      RunUntilIdle();
    }

    // fetch3_url
    {
      helper.StartUpdateCheck(std::move(fetch3));
      RunUntilIdle();
      const std::string kNoUpdate = CreateUpdateManifest(
          {UpdateManifestItem("3333")
               .version("3.0.0.0")
               .prodversionmin("3.0.0.0")
               .codebase("http://example.com/extension_3.0.0.0.crx")});
      helper.test_url_loader_factory().AddResponse(fetch3_url.spec(), kNoUpdate,
                                                   net::HTTP_OK);
      // The third fetcher doesn't have an update available.
      EXPECT_CALL(delegate, IsExtensionPending("3333")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("3333", _))
          .WillOnce(DoAll(SetArgPointee<1>("3.0.0.0"), Return(true)));
      EXPECT_CALL(
          delegate,
          OnExtensionDownloadFailed(
              "3333", ExtensionDownloaderDelegate::Error::NO_UPDATE_AVAILABLE,
              _, _, _))
          .WillOnce(InvokeWithoutArgs(&delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);
      fetch3_url = GURL();

      RunUntilIdle();
    }

    // fetch4_url
    {
      helper.StartUpdateCheck(std::move(fetch4));
      RunUntilIdle();
      // The last fetcher has an update.
      const std::string kUpdateAvailable = CreateUpdateManifest(
          {UpdateManifestItem("4444")
               .version("4.0.42.0")
               .prodversionmin("4.0.42.0")
               .codebase("http://example.com/extension_1.2.3.4.crx")});
      helper.test_url_loader_factory().AddResponse(
          fetch4_url.spec(), kUpdateAvailable, net::HTTP_OK);
      EXPECT_CALL(delegate, IsExtensionPending("4444")).WillOnce(Return(false));
      EXPECT_CALL(delegate, GetExtensionExistingVersion("4444", _))
          .WillOnce(DoAll(SetArgPointee<1>("4.0.0.0"), Return(true)));

      // Verify that the downloader decided to update this extension.
      EXPECT_CALL(delegate,
                  OnExtensionUpdateFound("4444", _, base::Version("4.0.42.0")))
          .WillOnce([&delegate]() { delegate.Quit(); });
      delegate.Wait();
      Mock::VerifyAndClearExpectations(&delegate);

      fetch4_url = GURL();
    }
    if (helper.downloader().HasActiveManifestRequestForTesting())
      ADD_FAILURE() << "Unexpected load";
  }

  void TestManifestRetryDownloading() {
    ExtensionDownloaderTestHelper helper;
    MockExtensionDownloaderDelegate& delegate = helper.delegate();
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zero_days(/*rollcall=*/0, /*active=*/0, /*enabled=*/true,
                               /*disable_reasons=*/{});
    fetch->AddExtension("1111", "1.0", &zero_days,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    // This will start the first fetcher.
    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    // ExtensionDownloader should retry kMaxRetries times and then fail.
    EXPECT_CALL(
        delegate,
        OnExtensionDownloadFailed(
            "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
            _, _, _));
    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_TRUE(request.load_flags == kExpectedLoadFlags);
          EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
                    request.credentials_mode);
        }));
    for (int i = 0; i <= ExtensionDownloader::kMaxRetries; ++i) {
      // All fetches will fail.
      auto* request = helper.GetPendingRequest(0);
      // Code 5xx causes ExtensionDownloader to retry.
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
      RunUntilIdle();
    }
    Mock::VerifyAndClearExpectations(&delegate);

    // For response codes that are not in the 5xx range ExtensionDownloader
    // should not retry.
    fetch.reset(CreateManifestFetchData(kUpdateUrl));
    fetch->AddExtension("1111", "1.0", &zero_days,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    // This will start the first fetcher.
    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    EXPECT_CALL(
        delegate,
        OnExtensionDownloadFailed(
            "1111", ExtensionDownloaderDelegate::Error::MANIFEST_FETCH_FAILED,
            _, _, _));

    // The first fetch will fail, and require retrying.
    {
      auto* request = helper.GetPendingRequest(0);
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    }
    RunUntilIdle();

    // The second fetch will fail with response 400 and should not cause
    // ExtensionDownloader to retry.
    {
      auto* request = helper.GetPendingRequest(0);
      helper.test_url_loader_factory().SimulateResponseForPendingRequest(
          request->request.url, network::URLLoaderCompletionStatus(net::OK),
          network::CreateURLResponseHead(net::HTTP_BAD_REQUEST), "");
    }
    RunUntilIdle();

    Mock::VerifyAndClearExpectations(&delegate);
  }

  void TestManifestCredentialsNonWebstore() {
    ExtensionDownloaderTestHelper helper;
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL kUpdateUrl("http://localhost/manifest1");

    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(kUpdateUrl));
    DownloadPingData zero_days(/*rollcall=*/0, /*active=*/0, /*enabled=*/true,
                               /*disable_reasons=*/{});
    fetch->AddExtension("1111", "1.0", &zero_days,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        DownloadFetchPriority::kBackground);

    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    helper.test_url_loader_factory().SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          EXPECT_EQ(network::mojom::CredentialsMode::kInclude,
                    request.credentials_mode);
        }));
    auto* request = helper.GetPendingRequest(0);
    helper.test_url_loader_factory().SimulateResponseForPendingRequest(
        request->request.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    RunUntilIdle();
  }

  // Checks that the manifest is fetched with a priority of |net::MEDIUM| (which
  // maps to |TaskPriority::USER_BLOCKING| for the task runner) when the active
  // request's |fetch_priority| is in the FOREGROUND.
  void TestManifestFetchPriority(DownloadFetchPriority fetch_priority) {
    ExtensionDownloaderTestHelper helper;
    helper.downloader().manifests_queue_.set_backoff_policy(kNoBackoffPolicy);
    GURL test_url("http://localhost/manifest1");
    std::unique_ptr<ManifestFetchData> fetch(
        CreateManifestFetchData(test_url));
    DownloadPingData zero_days(/*rollcall=*/0, /*active=*/0, /*enabled=*/true,
                               /*disable_reasons=*/{});

    fetch->AddExtension("1111", "1.0", &zero_days,
                        ExtensionDownloaderTestHelper::kEmptyUpdateUrlData,
                        std::string(), ManifestLocation::kInternal,
                        fetch_priority);

    helper.StartUpdateCheck(std::move(fetch));
    RunUntilIdle();

    auto* request = helper.GetPendingRequest(0);
    helper.test_url_loader_factory().SimulateResponseForPendingRequest(
        request->request.url, network::URLLoaderCompletionStatus(net::OK),
        network::CreateURLResponseHead(net::HTTP_INTERNAL_SERVER_ERROR), "");
    RunUntilIdle();

    if (fetch_priority == DownloadFetchPriority::kForeground) {
      EXPECT_EQ(net::MEDIUM, request->request.priority);
    } else {
      EXPECT_EQ(net::IDLE, request->request.priority);
    }
  }

  // Checks that the crx of a given extension is downloaded with a priority of
  // |net::MEDIUM| (maps to |TaskPriority::USER_BLOCKING| for the task runner)
  // when the active request's |fetch_priority| is in the FOREGROUND.
  void TestSingleExtensionDownloadingPriority(
      DownloadFetchPriority fetch_priority) {
    ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks;
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    MockExtensionDownloaderDelegate downloader_delegate;
    downloader_delegate.DelegateTo(&updater);
    factory.OverrideDownloaderDelegate(&downloader_delegate);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url("http://localhost/extension.crx");
    const std::string id(32, 'a');
    std::string hash;
    CRXFileInfo crx_file_info;
    base::Version version("0.0.1");
    ExtensionDownloaderTask task = CreateDownloaderTask(id);
    task.fetch_priority = fetch_priority;
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            std::move(task), test_url, hash, version.GetString(),
            fetch_priority);

    updater.downloader_->FetchUpdatedExtension(std::move(fetch), std::nullopt);

    auto* request = helper.GetPendingRequest(0);
    if (fetch_priority == DownloadFetchPriority::kForeground) {
      EXPECT_EQ(net::MEDIUM, request->request.priority);
    } else {
      EXPECT_EQ(net::IDLE, request->request.priority);
    }
  }

  void TestSingleExtensionDownloading(bool pending, bool retry, bool fail) {
    ExtensionUpdater::ScopedSkipScheduledCheckForTest skip_scheduled_checks;
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    MockExtensionDownloaderDelegate downloader_delegate;
    downloader_delegate.DelegateTo(&updater);
    factory.OverrideDownloaderDelegate(&downloader_delegate);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url("http://localhost/extension.crx");

    const std::string id(32, 'a');
    std::string hash;
    CRXFileInfo crx_file_info;
    base::Version version("0.0.1");
    std::set<int> requests;
    requests.insert(0);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id), test_url, hash, version.GetString(),
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(fetch), std::nullopt);

    if (pending) {
      const bool kIsFromSync = true;
      const bool kMarkAcknowledged = false;
      const bool kRemoteInstall = false;
      PendingExtensionManager* pending_extension_manager =
          PendingExtensionManager::Get(profile());
      pending_extension_manager->AddForTesting(PendingExtensionInfo(
          id, std::string(), test_url, version, &ShouldAlwaysInstall,
          kIsFromSync, ManifestLocation::kInternal, Extension::NO_FLAGS,
          kMarkAcknowledged, kRemoteInstall));
    }

    if (retry) {
      EXPECT_CALL(downloader_delegate, OnExtensionDownloadRetryForTests())
          .WillOnce(DoAll(
              InvokeWithoutArgs(&downloader_delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                             ClearURLLoaderFactoryResponses)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "", net::HTTP_INTERNAL_SERVER_ERROR);
      downloader_delegate.Wait();
      EXPECT_TRUE(updater.downloader_->extension_loader_);
    }

    if (fail) {
      EXPECT_CALL(downloader_delegate,
                  OnExtensionDownloadFailed(id, _, _, requests, _))
          .WillOnce(DoAll(
              InvokeWithoutArgs(&downloader_delegate,
                                &MockExtensionDownloaderDelegate::Quit),
              InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                             ClearURLLoaderFactoryResponses)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "Any content. It is irrelevant.",
          net::HTTP_NOT_FOUND);
      downloader_delegate.Wait();
    } else {
      EXPECT_TRUE(updater.downloader_->extension_loader_);
      EXPECT_CALL(downloader_delegate,
                  OnExtensionDownloadFinished_(_, _, _, _, requests, _))
          .WillOnce(
              DoAll(testing::SaveArg<0>(&crx_file_info),
                    InvokeWithoutArgs(&downloader_delegate,
                                      &MockExtensionDownloaderDelegate::Quit)));
      helper.test_url_loader_factory().AddResponse(
          test_url.spec(), "Any content. It is irrelevant.");
      downloader_delegate.Wait();
      EXPECT_EQ(version, crx_file_info.expected_version);
    }

    if (fail) {
      // Don't expect any extension to have been installed.
      EXPECT_TRUE(crx_installer_factory.crx_extension_id().empty());
    } else {
      // Expect that ExtensionUpdater asked the mock extensions service to
      // install a file with the test data for the right id.
      EXPECT_EQ(id, crx_file_info.extension_id);
      base::FilePath tmpfile_path = crx_file_info.path;
      EXPECT_FALSE(tmpfile_path.empty());
    }
  }

  // Helper to create a new test file of zeros.
  void CreateFile(const base::FilePath& file,
                  size_t size,
                  const base::Time& timestamp) {
    const std::string data(size, 0);
    EXPECT_TRUE(base::WriteFile(file, data));
    EXPECT_TRUE(base::TouchFile(file, timestamp, timestamp));
  }

  // Update a single extension in an environment where the download request
  // initially responds with a 403 status. The downloader then falls back onto
  // cookie-based credentials, which are only used over HTTPS, and it never
  // sends an Authorization header. If |succeed_with_cookies| is false, the
  // download fails.
  void TestProtectedDownload(const std::string& url_prefix,
                             bool succeed_with_cookies) {
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);

    MockExtensionDownloaderDelegate downloader_delegate;
    downloader_delegate.DelegateTo(&updater);
    factory.OverrideDownloaderDelegate(&downloader_delegate);

    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    GURL test_url(base::StringPrintf("%s/extension.crx", url_prefix.c_str()));

    const std::string id(32, 'a');
    std::string hash;
    base::Version version("0.0.1");
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> extension_fetch =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id), test_url, hash, version.GetString(),
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(extension_fetch),
                                               std::nullopt);

    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    // Fake a 403 response.
    EXPECT_CALL(downloader_delegate, OnExtensionDownloadRetryForTests())
        .WillOnce(DoAll(
            InvokeWithoutArgs(&downloader_delegate,
                              &MockExtensionDownloaderDelegate::Quit),
            InvokeWithoutArgs(&helper, &ExtensionDownloaderTestHelper::
                                           ClearURLLoaderFactoryResponses)));
    helper.test_url_loader_factory().AddResponse(test_url.spec(), "",
                                                 net::HTTP_FORBIDDEN);
    downloader_delegate.Wait();

    // Verify that the fetch has had its credentials properly incremented.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    net::HttpRequestHeaders fetch_headers =
        updater.downloader_
            ->last_extension_loader_resource_request_headers_for_testing_;
    EXPECT_FALSE(
        fetch_headers.HasHeader(net::HttpRequestHeaders::kAuthorization));
    EXPECT_EQ(
        kExpectedLoadFlagsForDownloadWithCookies,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    const ExtensionDownloader::ExtensionFetch& fetch =
        *updater.downloader_->extensions_queue_.active_request();

    if (!succeed_with_cookies) {
      // The retry is the last attempt.
      helper.test_url_loader_factory().AddResponse(
          fetch.url.spec(), std::string(), net::HTTP_UNAUTHORIZED);
      EXPECT_CALL(downloader_delegate, OnExtensionDownloadFailed(_, _, _, _, _))
          .WillOnce(InvokeWithoutArgs(&downloader_delegate,
                                      &MockExtensionDownloaderDelegate::Quit));
      downloader_delegate.Wait();
      return;
    }

    // Simulate successful authorization with a 200 response.
    CRXFileInfo crx_file_info;
    EXPECT_CALL(downloader_delegate,
                OnExtensionDownloadFinished_(_, _, _, _, _, _))
        .WillOnce(
            DoAll(testing::SaveArg<0>(&crx_file_info),
                  InvokeWithoutArgs(&downloader_delegate,
                                    &MockExtensionDownloaderDelegate::Quit)));
    helper.test_url_loader_factory().AddResponse(fetch.url.spec(), "whatever");
    downloader_delegate.Wait();

    // Verify installation would proceed as normal.
    EXPECT_EQ(id, crx_file_info.extension_id);
    base::FilePath tmpfile_path = crx_file_info.path;
    EXPECT_FALSE(tmpfile_path.empty());
  }

  // Two extensions are updated.  If |updates_start_running| is true, the
  // mock extensions service has CreateUpdateInstaller(...) return the
  // fake CrxInstallers created by the test.  Otherwise, CreateUpdateInstaller()
  // returns nullptr, signaling install failures.
  void TestMultipleExtensionDownloading(bool updates_start_running) {
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    updater.Start();
    updater.EnsureDownloaderCreated();
    updater.downloader_->extensions_queue_.set_backoff_policy(kNoBackoffPolicy);

    EXPECT_THAT(GetRunningInstallIds(updater), testing::IsEmpty());

    GURL url1("http://localhost/extension1.crx");
    GURL url2("http://localhost/extension2.crx");

    const std::string id1(32, 'a');
    const std::string id2(32, 'b');

    std::string hash1;
    std::string hash2;

    std::string version1 = "0.1";
    std::string version2 = "0.1";

    // Start two fetches
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch1 =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id1), url1, hash1, version1,
            DownloadFetchPriority::kBackground);
    std::unique_ptr<ExtensionDownloader::ExtensionFetch> fetch2 =
        std::make_unique<ExtensionDownloader::ExtensionFetch>(
            CreateDownloaderTask(id2), url2, hash2, version2,
            DownloadFetchPriority::kBackground);
    updater.downloader_->FetchUpdatedExtension(std::move(fetch1),
                                               std::optional<std::string>());
    updater.downloader_->FetchUpdatedExtension(std::move(fetch2),
                                               std::optional<std::string>());

    // Make the first fetch complete.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    // We need some CrxInstallers, and CrxInstallers require a real
    // ExtensionService.  Create one on the testing profile.  Any action
    // the CrxInstallers take is on the testing profile's extension
    // service, not on our mock |service|.  This allows us to fake
    // the CrxInstaller actions we want.
    TestingProfile profile;
    // TODO(crbug.com/396722906): Delete this when CrxInstaller no longer has
    // dependencies on ExtensionService.
    static_cast<TestExtensionSystem*>(ExtensionSystem::Get(&profile))
        ->CreateExtensionService(base::CommandLine::ForCurrentProcess(),
                                 base::FilePath(), false);

    scoped_refptr<FakeCrxInstaller> fake_crx1 =
        base::MakeRefCounted<FakeCrxInstaller>(&profile);
    scoped_refptr<FakeCrxInstaller> fake_crx2 =
        base::MakeRefCounted<FakeCrxInstaller>(&profile);

    if (updates_start_running) {
      // Add mock CrxInstaller to be returned by
      // delegate.CreateUpdateInstaller().
      crx_installer_factory.AddFakeCrxInstaller(id1, fake_crx1);
      crx_installer_factory.AddFakeCrxInstaller(id2, fake_crx2);
    } else {
      // If we don't add mock CRX installers, the mock delegate will just return
      // nullptr, meaning a failure.
    }

    helper.test_url_loader_factory().AddResponse(
        url1.spec(), "Any content. This is irrelevant.", net::HTTP_OK);
    content::RunAllTasksUntilIdle();

    // Expect that the delegate was asked to do an install with the right data.
    base::FilePath tmpfile_path = crx_installer_factory.crx_install_path();
    EXPECT_FALSE(tmpfile_path.empty());
    EXPECT_EQ(id1, crx_installer_factory.crx_extension_id());
    RunUntilIdle();

    // Make sure the second fetch finished and asked the service to do an
    // update.
    EXPECT_TRUE(updater.downloader_->extension_loader_);
    EXPECT_EQ(
        kExpectedLoadFlags,
        updater.downloader_->last_extension_loader_load_flags_for_testing_);

    helper.test_url_loader_factory().AddResponse(
        url2.spec(), "Any other content. This is irrelevant.", net::HTTP_OK);
    content::RunAllTasksUntilIdle();

    if (updates_start_running) {
      // Both installations should have launched in parallel.
      EXPECT_THAT(GetRunningInstallIds(updater),
                  testing::UnorderedElementsAre(id1, id2));

      // Fake install notice.  This should end the first installation.
      fake_crx1->RunInstallerCallbacks(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));
      content::RunAllTasksUntilIdle();
      EXPECT_THAT(GetRunningInstallIds(updater),
                  testing::UnorderedElementsAre(id2));

      fake_crx2->RunInstallerCallbacks(CrxInstallError(
          CrxInstallErrorType::OTHER, CrxInstallErrorDetail::NONE));
      content::RunAllTasksUntilIdle();
    }
    EXPECT_THAT(GetRunningInstallIds(updater), testing::IsEmpty());
  }

  // This makes sure that the extension updater ignores a <daystart> tag from a
  // manifest fetch, even from a google.com update URL: no ping is sent, so
  // there is no ping day to store.
  void TestHandleManifestResults() {
    ExtensionDownloaderTestHelper helper;
    TestDownloaderFactory factory(helper.url_loader_factory());
    TestCrxInstallerFactory crx_installer_factory;
    GURL update_url("http://www.google.com/manifest");
    ExtensionList tmp;
    CreateTestExtensions(1, 1, &tmp, &update_url.spec(),
                         ManifestLocation::kInternal);
    SetExtensions(tmp, ExtensionList());

    ExtensionUpdater updater(profile());
    updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                          nullptr, factory.GetDownloaderFactory());
    updater.set_crx_installer_factory_for_test(&crx_installer_factory);
    updater.Start();
    updater.EnsureDownloaderCreated();

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(update_url));
    const Extension* extension = tmp[0].get();
    AddExtensionToFetchDataForTesting(fetch_data.get(), extension->id(),
                                      extension->VersionString(), update_url);
    auto results = std::make_unique<UpdateManifestResults>();
    results->daystart_elapsed_seconds = 750;

    updater.downloader_->HandleManifestResults(std::move(fetch_data),
                                               std::move(results),
                                               /*error=*/std::nullopt);
    EXPECT_TRUE(extension_prefs()->LastPingDay(extension->id()).is_null());
  }

  void TestManifestAddExtension(DownloadFetchPriority data_priority,
                                DownloadFetchPriority extension_priority,
                                DownloadFetchPriority expected_priority) {
    const std::string id(32, 'a');
    const std::string version = "1.0";

    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));
    ASSERT_TRUE(fetch_data->AddExtension(
        id, version, ExtensionDownloaderTestHelper::GetTestPingData(),
        std::string(), std::string(), ManifestLocation::kInternal,
        extension_priority));
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

  void TestManifestMerge(DownloadFetchPriority data_priority,
                         DownloadFetchPriority other_priority,
                         DownloadFetchPriority expected_priority) {
    std::unique_ptr<ManifestFetchData> fetch_data(
        CreateManifestFetchData(GURL("http://localhost/foo"), data_priority));

    std::unique_ptr<ManifestFetchData> fetch_other(
        CreateManifestFetchData(GURL("http://localhost/foo"), other_priority));

    fetch_data->Merge(std::move(fetch_other));
    ASSERT_EQ(expected_priority, fetch_data->fetch_priority());
  }

 protected:
  std::unique_ptr<TestExtensionPrefs> prefs_;
  content::BrowserTaskEnvironment task_environment_;

  ManifestFetchData* CreateManifestFetchData(
      const GURL& update_url,
      DownloadFetchPriority fetch_priority) {
    return new ManifestFetchData(update_url, 0, "",
                                 UpdateQueryParams::Get(UpdateQueryParams::CRX),
                                 ManifestFetchData::PING, fetch_priority);
  }

  ManifestFetchData* CreateManifestFetchData(const GURL& update_url) {
    return CreateManifestFetchData(update_url,
                                   DownloadFetchPriority::kBackground);
  }

 private:
  content::InProcessUtilityThreadHelper in_process_utility_thread_helper_;

  StubExtensionRegistrarDelegate stub_extension_registrar_delegate_;
};

// Because we test some private methods of ExtensionUpdater, it's easier for the
// actual test code to live in ExtensionUpdaterTest methods instead of TEST_F
// subclasses where friendship with ExtensionUpdater is not inherited.

TEST_F(ExtensionUpdaterTest, TestExtensionUpdateCheckRequests) {
  TestExtensionUpdateCheckRequests(false);
}

TEST_F(ExtensionUpdaterTest, TestExtensionUpdateCheckRequestsPending) {
  TestExtensionUpdateCheckRequests(true);
}

TEST_F(ExtensionUpdaterTest, TestUpdateUrlData) {
  TestUpdateUrlDataEmpty();
  TestUpdateUrlDataSimple();
  TestUpdateUrlDataCompound();
  TestUpdateUrlDataFromUrl("http://example.com/update",
                           DownloadFetchPriority::kForeground, 4);
}

TEST_F(ExtensionUpdaterTest, TestInstallSource) {
  TestInstallSource();
}

TEST_F(ExtensionUpdaterTest, TestInstallLocation) {
  TestInstallLocation();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdates) {
  TestDetermineUpdates();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesPending) {
  TestDetermineUpdatesPending();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesDuplicates) {
  TestDetermineUpdatesDuplicates();
}

TEST_F(ExtensionUpdaterTest, TestDetermineUpdatesError) {
  TestDetermineUpdatesError();
}

TEST_F(ExtensionUpdaterTest, TestMultipleManifestDownloading) {
  TestMultipleManifestDownloading();
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloading) {
  TestSingleExtensionDownloading(false, false, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingPending) {
  TestSingleExtensionDownloading(true, false, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingWithRetry) {
  TestSingleExtensionDownloading(false, true, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingPendingWithRetry) {
  TestSingleExtensionDownloading(true, true, false);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailure) {
  TestSingleExtensionDownloading(false, false, true);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailureWithRetry) {
  TestSingleExtensionDownloading(false, true, true);
}

TEST_F(ExtensionUpdaterTest, TestSingleExtensionDownloadingFailurePending) {
  TestSingleExtensionDownloading(true, false, true);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadCookieAuth) {
  TestProtectedDownload("https://example.com/download",
                        /*succeed_with_cookies=*/true);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadCookieFailure) {
  TestProtectedDownload("https://example.com/download",
                        /*succeed_with_cookies=*/false);
}

TEST_F(ExtensionUpdaterTest, ProtectedDownloadFailWithoutHTTPS) {
  TestProtectedDownload("http://example.com", /*succeed_with_cookies=*/true);
}

TEST_F(ExtensionUpdaterTest, TestMultipleExtensionDownloadingUpdatesFail) {
  TestMultipleExtensionDownloading(false);
}
TEST_F(ExtensionUpdaterTest, TestMultipleExtensionDownloadingUpdatesSucceed) {
  TestMultipleExtensionDownloading(true);
}

TEST_F(ExtensionUpdaterTest, TestManifestRetryDownloading) {
  TestManifestRetryDownloading();
}

TEST_F(ExtensionUpdaterTest, TestHandleManifestResults) {
  TestHandleManifestResults();
}

TEST_F(ExtensionUpdaterTest, TestNonAutoUpdateableLocations) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);
  MockExtensionDownloaderDelegate downloader_delegate;
  factory.OverrideDownloaderDelegate(&downloader_delegate);

  // Non-internal non-external extensions should be rejected.
  ExtensionList extensions;
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &extensions, &update_url,
                       ManifestLocation::kInvalidLocation);
  ASSERT_EQ(1u, extensions.size());
  // The test will fail with unexpected calls if the delegate's methods are
  // invoked for the extension.
  SetExtensions(extensions, ExtensionList());
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
}

TEST_F(ExtensionUpdaterTest, TestUpdatingDisabledExtensions) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);

  // Non-internal non-external extensions should be rejected.
  ExtensionList enabled_extensions;
  ExtensionList disabled_extensions;
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &enabled_extensions, &update_url,
                       ManifestLocation::kInternal);
  CreateTestExtensions(2, 1, &disabled_extensions, &update_url,
                       ManifestLocation::kInternal);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(1u, disabled_extensions.size());

  SetExtensions(enabled_extensions, disabled_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());

  // We expect that both enabled and disabled extensions are auto-updated.
  EXPECT_THAT(GetActiveManifestFetchIds(&updater),
              testing::UnorderedElementsAre(enabled_extensions[0]->id(),
                                            disabled_extensions[0]->id()));
}

// crbug.com/40137079: Tests that removely disabled extensions that are part of
// the blocklisted extensions are still receive updates.
TEST_F(ExtensionUpdaterTest, TestUpdatingRemotelyDisabledExtensions) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);

  ExtensionList enabled_extensions;
  ExtensionList blocklisted_extensions;
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &enabled_extensions, &update_url,
                       ManifestLocation::kInternal);
  CreateTestExtensions(2, 1, &blocklisted_extensions, &update_url,
                       ManifestLocation::kInternal);
  CreateTestExtensions(3, 1, &blocklisted_extensions, &update_url,
                       ManifestLocation::kInternal);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(2u, blocklisted_extensions.size());
  const std::string& remotely_blocklisted_id = blocklisted_extensions[0]->id();
  blocklist_prefs::SetSafeBrowsingExtensionBlocklistState(
      remotely_blocklisted_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());
  blocklist_prefs::AddOmahaBlocklistState(
      remotely_blocklisted_id, BitMapBlocklistState::BLOCKLISTED_MALWARE,
      extension_prefs());

  SetExtensions(enabled_extensions, ExtensionList(), blocklisted_extensions);
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());

  // We expect that both enabled and remotely blocklisted extensions are
  // auto-updated.
  EXPECT_THAT(GetActiveManifestFetchIds(&updater),
              testing::UnorderedElementsAre(enabled_extensions[0]->id(),
                                            blocklisted_extensions[0]->id(),
                                            blocklisted_extensions[1]->id()));
}

TEST_F(ExtensionUpdaterTest, TestPendingInstall) {
  // Add an extension as a pending update with a higher version number.
  base::DictValue manifest;
  manifest.Set(manifest_keys::kKey, kExtensionManifestKey);
  manifest.Set(manifest_keys::kName, "Fake extension");
  manifest.Set(manifest_keys::kVersion, "1.0.0.1");
  manifest.Set(manifest_keys::kManifestVersion, 2);
  scoped_refptr<Extension> pending_update =
      prefs_->AddExtensionWithManifest(manifest, ManifestLocation::kInternal);
  DelayedInstallManager::Get(profile())->Insert(pending_update);

  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);

  ExtensionList enabled_extensions;
  std::string extension_key(kExtensionManifestKey);
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &enabled_extensions, &update_url,
                       ManifestLocation::kInternal, extension_key);
  ASSERT_EQ(1u, enabled_extensions.size());
  ASSERT_EQ(enabled_extensions[0]->VersionString(), "1.0.0.0");

  SetExtensions(enabled_extensions, ExtensionList());
  updater.Start();

  // When an update manifest is compared, we expect the pending version is used.
  std::string existing_version;
  ASSERT_TRUE(GetExtensionExistingVersion(&updater, enabled_extensions[0]->id(),
                                          &existing_version));
  EXPECT_EQ("1.0.0.1", existing_version);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchesBuilderAddExtension) {
  auto helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  // First, verify that adding valid extensions does invoke the callbacks on
  // the delegate.
  std::string id = crx_file::id_util::GenerateId("foo");
  EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
      .WillOnce(Return(false));
  EXPECT_TRUE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL("http://example.com/update"))));
  helper->downloader().StartAllPending(nullptr);
  Mock::VerifyAndClearExpectations(&helper->delegate());
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with invalid update URLs should be rejected.
  id = crx_file::id_util::GenerateId("foo2");
  EXPECT_FALSE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL("http:google.com:foo"))));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with empty IDs should be rejected.
  EXPECT_FALSE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(std::string(), GURL())));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));

  // TODO(akalin): Test that extensions with empty update URLs
  // converted from user scripts are rejected.

  // Reset the ExtensionDownloader so that it drops the current fetcher.
  helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  // Extensions with empty update URLs should be rejected.
  id = crx_file::id_util::GenerateId("foo3");
  EXPECT_FALSE(helper->downloader().AddPendingExtension(
      CreateDownloaderTask(id, GURL())));
  helper->downloader().StartAllPending(nullptr);
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));
}

TEST_F(ExtensionUpdaterTest, TestAddPendingExtensionWithVersion) {
  constexpr char kVersion[] = "1.2.3.4";
  auto helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  {
    // First, verify that adding valid extensions does invoke the callbacks on
    // the delegate.
    ExtensionId id = crx_file::id_util::GenerateId("foo");
    EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
        .WillOnce(Return(false));
    ExtensionDownloaderTask task =
        CreateDownloaderTask(id, GURL("http://example.com/update"));
    task.version = base::Version(kVersion);
    EXPECT_TRUE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    Mock::VerifyAndClearExpectations(&helper->delegate());
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  {
    // Extensions with invalid update URLs should be rejected.
    ExtensionId id = crx_file::id_util::GenerateId("foo2");
    ExtensionDownloaderTask task =
        CreateDownloaderTask(id, GURL("http:google.com:foo"));
    task.version = base::Version(kVersion);
    EXPECT_FALSE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  {
    // Extensions with empty IDs should be rejected.
    ExtensionDownloaderTask task = CreateDownloaderTask(std::string(), GURL());
    task.version = base::Version(kVersion);
    EXPECT_FALSE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  // Reset the ExtensionDownloader so that it drops the current fetcher.
  helper = std::make_unique<ExtensionDownloaderTestHelper>();
  EXPECT_EQ(0u, ManifestFetchersCount(&helper->downloader()));

  {
    // The version of a pending extension reaches the update server.
    ExtensionId id = crx_file::id_util::GenerateId("foo3");
    EXPECT_CALL(helper->delegate(), GetPingDataForExtension(id, _))
        .WillOnce(Return(false));
    ExtensionDownloaderTask task =
        CreateDownloaderTask(id, GURL("http://example.com/update"));
    task.version = base::Version(kVersion);
    EXPECT_TRUE(helper->downloader().AddPendingExtension(std::move(task)));
    helper->downloader().StartAllPending(nullptr);
    EXPECT_EQ(1u, ManifestFetchersCount(&helper->downloader()));
  }

  RunUntilIdle();
  auto* request = &(*helper->test_url_loader_factory().pending_requests())[0];
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->request.url.is_empty());
  EXPECT_THAT(request->request.url.possibly_invalid_spec(),
              testing::HasSubstr(kVersion));
}

TEST_F(ExtensionUpdaterTest, TestStartUpdateCheckMemory) {
  ExtensionDownloaderTestHelper helper;

  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should delete the newly-created ManifestFetchData.
  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://localhost/foo")));
  // This should add into |manifests_pending_|.
  StartUpdateCheck(&helper.downloader(),
                   CreateManifestFetchData(GURL("http://www.google.com")));
  // The dtor of |downloader| should delete the pending fetchers.
}

TEST_F(ExtensionUpdaterTest, TestCheckSoon) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.Start();
  EXPECT_TRUE(updater.WillCheckSoon());
  RunUntilIdle();
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  RunUntilIdle();
  EXPECT_FALSE(updater.WillCheckSoon());
  updater.CheckSoon();
  EXPECT_TRUE(updater.WillCheckSoon());
  updater.Stop();
  EXPECT_FALSE(updater.WillCheckSoon());
}

// Verifies that HasFullCheckInProgress() distinguishes full update checks from
// targeted single-extension update checks, remaining false during targeted
// checks and transitioning to true during full checks.
TEST_F(ExtensionUpdaterTest, TestHasFullCheckInProgress) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionList tmp;
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(2, 2, &tmp, &update_url, ManifestLocation::kInternal);
  SetExtensions(tmp, ExtensionList());

  ASSERT_EQ(2u, tmp.size());
  ExtensionId id1 = tmp[0]->id();
  ExtensionId id2 = tmp[1]->id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(id1));
  ASSERT_TRUE(registry->enabled_extensions().GetByID(id2));

  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);

  updater.Start();
  EXPECT_FALSE(updater.HasFullCheckInProgress());

  // A targeted check must not report as a full check.
  ExtensionUpdater::CheckParams params;
  params.ids = {id1};
  updater.CheckNow(std::move(params));
  EXPECT_FALSE(updater.HasFullCheckInProgress());

  // A parameterless check covers all extensions and must report as a full
  // check.
  updater.CheckNow(ExtensionUpdater::CheckParams());
  EXPECT_TRUE(updater.HasFullCheckInProgress());

  updater.Stop();
  EXPECT_FALSE(updater.HasFullCheckInProgress());
}

TEST_F(ExtensionUpdaterTest, TestUninstallWhileUpdateCheck) {
  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  TestCrxInstallerFactory crx_installer_factory;
  ExtensionList tmp;
  const std::string update_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &tmp, &update_url, ManifestLocation::kInternal);
  SetExtensions(tmp, ExtensionList());

  ASSERT_EQ(1u, tmp.size());
  ExtensionId id = tmp.front()->id();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ASSERT_TRUE(registry->enabled_extensions().GetByID(id));

  ExtensionUpdater updater(profile());
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.set_crx_installer_factory_for_test(&crx_installer_factory);
  ExtensionUpdater::CheckParams params;
  params.ids = {id};
  updater.Start();
  updater.CheckNow(std::move(params));

  SetExtensions(ExtensionList(), ExtensionList());
  ASSERT_FALSE(registry->enabled_extensions().GetByID(id));

  // RunUntilIdle is needed to make sure that the extension update process has a
  // chance to exit gracefully.
  RunUntilIdle();
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataAddExtension) {
  TestManifestAddExtension(DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kBackground);

  TestManifestAddExtension(DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground);

  TestManifestAddExtension(DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kBackground,
                           DownloadFetchPriority::kForeground);

  TestManifestAddExtension(DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground,
                           DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchDataMerge) {
  TestManifestMerge(DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kBackground);

  TestManifestMerge(DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground);

  TestManifestMerge(DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kBackground,
                    DownloadFetchPriority::kForeground);

  TestManifestMerge(DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground,
                    DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchCredentials) {
  TestManifestCredentialsNonWebstore();
}

TEST_F(ExtensionUpdaterTest, TestManifestFetchPriority) {
  TestManifestFetchPriority(DownloadFetchPriority::kBackground);
  TestManifestFetchPriority(DownloadFetchPriority::kForeground);
}

TEST_F(ExtensionUpdaterTest, TestExtensionPriority) {
  TestSingleExtensionDownloadingPriority(DownloadFetchPriority::kBackground);
  TestSingleExtensionDownloadingPriority(DownloadFetchPriority::kForeground);
}

// An extension with an empty or a store update URL is left out of update
// checks: no request goes out and the check ends right away.
TEST_F(ExtensionUpdaterTest, NoUpdateRequestForInstalledEmptyUpdateUrl) {
  ExtensionList extensions;
  CreateTestExtensions(1, 1, &extensions, nullptr, ManifestLocation::kInternal);
  SetExtensions(extensions, ExtensionList());
  EXPECT_THAT(GetIdsOfFullUpdateCheck(), testing::IsEmpty());
}

TEST_F(ExtensionUpdaterTest, NoUpdateRequestForInstalledStoreUpdateUrl) {
  ExtensionList extensions;
  const std::string update_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();
  CreateTestExtensions(1, 1, &extensions, &update_url,
                       ManifestLocation::kInternal);
  SetExtensions(extensions, ExtensionList());
  EXPECT_THAT(GetIdsOfFullUpdateCheck(), testing::IsEmpty());
}

TEST_F(ExtensionUpdaterTest, NoUpdateRequestForPendingEmptyUpdateUrl) {
  SetupPendingExtensionManagerForTest(1, GURL(),
                                      PendingExtensionManager::Get(profile()));
  EXPECT_THAT(GetIdsOfFullUpdateCheck(), testing::IsEmpty());
}

TEST_F(ExtensionUpdaterTest, NoUpdateRequestForPendingStoreUpdateUrl) {
  SetupPendingExtensionManagerForTest(
      1, extension_urls::GetDefaultWebstoreUpdateUrl(),
      PendingExtensionManager::Get(profile()));
  EXPECT_THAT(GetIdsOfFullUpdateCheck(), testing::IsEmpty());
}

// The control case of the tests above: of everything that is set up, only the
// extension with a third-party update URL takes part in the check.
TEST_F(ExtensionUpdaterTest, UpdateRequestForThirdPartyUpdateUrlOnly) {
  ExtensionList extensions;
  const std::string store_url =
      extension_urls::GetDefaultWebstoreUpdateUrl().spec();
  const std::string third_party_url(kThirdPartyUpdateUrl);
  CreateTestExtensions(1, 1, &extensions, nullptr, ManifestLocation::kInternal);
  CreateTestExtensions(2, 1, &extensions, &store_url,
                       ManifestLocation::kInternal);
  CreateTestExtensions(3, 1, &extensions, &third_party_url,
                       ManifestLocation::kInternal);
  SetExtensions(extensions, ExtensionList());
  SetupPendingExtensionManagerForTest(
      1, extension_urls::GetDefaultWebstoreUpdateUrl(),
      PendingExtensionManager::Get(profile()));
  EXPECT_THAT(GetIdsOfFullUpdateCheck(),
              testing::ElementsAre(extensions[2]->id()));
}

TEST_F(ExtensionUpdaterTest, TestProfileDestruction) {
  ExtensionUpdater updater(profile());
  // Create an active ProfileManager, and do NOT make it the owner of profile().
  // This causes ScopedProfileKeepAlive::TryAcquire() to fail.
  auto testing_profile_manager = std::make_unique<TestingProfileManager>(
      TestingBrowserProcess::GetGlobal());
  ASSERT_TRUE(testing_profile_manager->SetUp());

  ExtensionDownloaderTestHelper helper;
  TestDownloaderFactory factory(helper.url_loader_factory());
  // Verify that CheckNow() doesn't do anything in this state.
  updater.SetUpdatingStartedCallbackForTesting(base::BindLambdaForTesting(
      [&]() { ADD_FAILURE() << "Updating should not have started."; }));
  updater.InitAndEnable(extension_prefs(), pref_service(), kUpdateFrequency,
                        nullptr, factory.GetDownloaderFactory());
  updater.Start();
  updater.CheckNow(ExtensionUpdater::CheckParams());
  base::RunLoop().RunUntilIdle();
}

// TODO(asargent) - (http://crbug.com/40809955) add tests for:
// -prodversionmin (shouldn't update if browser version too old)
// -manifests & updates arriving out of order / interleaved
// -malformed update url (empty, file://, has query, has a # fragment, etc.)
// -An extension gets uninstalled while updates are in progress (so it doesn't
//  "come back from the dead")
// -An extension gets manually updated to v3 while we're downloading v2 (ie
//  you don't get downgraded accidentally)
// -An update manifest mentions multiple updates

}  // namespace extensions
