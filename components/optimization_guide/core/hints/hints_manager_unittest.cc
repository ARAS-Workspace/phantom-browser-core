// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints/hints_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/to_string.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/filters/bloom_filter.h"
#include "components/optimization_guide/core/filters/hints_component_util.h"
#include "components/optimization_guide/core/hints/hint_cache.h"
#include "components/optimization_guide/core/hints/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/hints/optimization_guide_store.h"
#include "components/optimization_guide/core/hints/tab_url_provider.h"
#include "components/optimization_guide/core/hints/top_host_provider.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/proto_database_provider_test_base.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/test_identity_manager_observer.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {
namespace {

// Allows for default hour to pass + random delay between 30 and 60 seconds.
constexpr int kUpdateFetchHintsTimeSecs = 61 * 60;  // 1 hours and 1 minutes.

const int kDefaultHostBloomFilterNumHashFunctions = 7;
const int kDefaultHostBloomFilterNumBits = 511;

void PopulateBloomFilterWithDefaultHost(BloomFilter* bloom_filter) {
  bloom_filter->Add("host.com");
}

void AddBloomFilterToConfig(proto::OptimizationType optimization_type,
                            const BloomFilter& bloom_filter,
                            int num_hash_functions,
                            int num_bits,
                            bool is_allowlist,
                            proto::Configuration* config) {
  std::string bloom_filter_data(
      reinterpret_cast<const char*>(&bloom_filter.bytes()[0]),
      bloom_filter.bytes().size());
  proto::OptimizationFilter* of_proto =
      is_allowlist ? config->add_optimization_allowlists()
                   : config->add_optimization_blocklists();
  of_proto->set_optimization_type(optimization_type);
  std::unique_ptr<proto::BloomFilter> bloom_filter_proto =
      std::make_unique<proto::BloomFilter>();
  bloom_filter_proto->set_num_hash_functions(num_hash_functions);
  bloom_filter_proto->set_num_bits(num_bits);
  bloom_filter_proto->set_data(bloom_filter_data);
  of_proto->set_allocated_bloom_filter(bloom_filter_proto.release());
}

std::unique_ptr<proto::GetHintsResponse> BuildHintsResponse(
    const std::vector<std::string>& hosts,
    const std::vector<std::string>& urls) {
  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  for (const auto& host : hosts) {
    proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(proto::HOST);
    hint->set_key(host);
    hint->add_allowlisted_optimizations()->set_optimization_type(
        proto::NOSCRIPT);
    proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern("page pattern");
    proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::DEFER_ALL_SCRIPT);
  }
  for (const auto& url : urls) {
    proto::Hint* hint = get_hints_response->add_hints();
    hint->set_key_representation(proto::FULL_URL);
    hint->set_key(url);
    hint->mutable_max_cache_duration()->set_seconds(60 * 60);
    proto::PageHint* page_hint = hint->add_page_hints();
    page_hint->set_page_pattern(url);
    proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::COMPRESS_PUBLIC_IMAGES);
    opt->mutable_any_metadata()->set_type_url("someurl");
  }
  return get_hints_response;
}

void RunHintsFetchedCallbackWithResponse(
    HintsFetchedCallback hints_fetched_callback,
    std::unique_ptr<proto::GetHintsResponse> response) {
  std::move(hints_fetched_callback).Run(std::move(response));
}

// Returns the default params used for the kOptimizationHints feature.
base::FieldTrialParams GetOptimizationHintsDefaultFeatureParams() {
  return {{
      "max_host_keyed_hint_cache_size",
      "1",
  }};
}

std::unique_ptr<base::test::ScopedFeatureList>
SetUpDeferStartupActiveTabsHintsFetch(bool is_enabled) {
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_list =
      std::make_unique<base::test::ScopedFeatureList>();
  auto params = GetOptimizationHintsDefaultFeatureParams();

  params["defer_startup_active_tabs_hints_fetch"] = base::ToString(is_enabled);
  scoped_feature_list->InitAndEnableFeatureWithParameters(
      features::kOptimizationHints, params);
  return scoped_feature_list;
}

}  // namespace

// A mock class implementation of TopHostProvider.
class FakeTopHostProvider : public TopHostProvider {
 public:
  explicit FakeTopHostProvider(const std::vector<std::string>& top_hosts)
      : top_hosts_(top_hosts) {}

  std::vector<std::string> GetTopHosts() override {
    num_top_hosts_called_++;

    return top_hosts_;
  }

  int get_num_top_hosts_called() const { return num_top_hosts_called_; }

 private:
  std::vector<std::string> top_hosts_;
  int num_top_hosts_called_ = 0;
};

// A mock class implementation of TabUrlProvider.
class FakeTabUrlProvider : public TabUrlProvider {
 public:
  const std::vector<GURL> GetUrlsOfActiveTabs(
      const base::TimeDelta& duration_since_last_shown) override {
    num_urls_called_++;
    return urls_;
  }

  void SetUrls(const std::vector<GURL>& urls) { urls_ = urls; }

  int get_num_urls_called() const { return num_urls_called_; }

 private:
  std::vector<GURL> urls_;
  int num_urls_called_ = 0;
};

class HintsManagerTest : public ProtoDatabaseProviderTestBase {
 public:
  HintsManagerTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kOptimizationHints,
             GetOptimizationHintsDefaultFeatureParams()},
            {kHintsBatchUpdateForActiveTabsAndTopHosts, {}},
        },
        /*disabled_features=*/{});

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    prefs::RegisterProfilePrefs(pref_service_->registry());
    unified_consent::UnifiedConsentService::RegisterPrefs(
        pref_service_->registry());
  }
  ~HintsManagerTest() override = default;

  HintsManagerTest(const HintsManagerTest&) = delete;
  HintsManagerTest& operator=(const HintsManagerTest&) = delete;

  void SetUp() override {
    ProtoDatabaseProviderTestBase::SetUp();
    CreateHintsManager(/*top_host_provider=*/nullptr);
  }

  void TearDown() override {
    ResetHintsManager();
    pref_service_.reset();
    ProtoDatabaseProviderTestBase::TearDown();
  }

  void CreateHintsManager(
      std::unique_ptr<FakeTopHostProvider> top_host_provider,
      signin::IdentityManager* identity_manager = nullptr) {
    if (hints_manager_) {
      ResetHintsManager();
    }

    url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    hint_store_ = std::make_unique<OptimizationGuideStore>(
        db_provider_.get(), temp_dir(),
        task_environment_.GetMainThreadTaskRunner());

    tab_url_provider_ = std::make_unique<FakeTabUrlProvider>();

    top_host_provider_ = std::move(top_host_provider);

    hints_manager_ = std::make_unique<HintsManager>(
        /*is_off_the_record=*/false, /*application_locale=*/"en-US",
        pref_service(), hint_store_->AsWeakPtr(), top_host_provider_.get(),
        tab_url_provider_.get(), url_loader_factory_,
        /*push_notification_manager=*/nullptr,
        /*identity_manager=*/identity_manager, &optimization_guide_logger_);
    hints_manager_->SetClockForTesting(task_environment_.GetMockClock());

    // Run until hint cache is initialized and the HintsManager is ready to
    // process hints.
    RunUntilIdle();
  }

  void ResetHintsManager() {
    hints_manager_->Shutdown();
    hints_manager_.reset();
    tab_url_provider_.reset();
    if (top_host_provider_) {
      top_host_provider_.reset();
    }
    hint_store_.reset();
    RunUntilIdle();
  }

  void ProcessInvalidHintsComponentInfo(const std::string& version) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("notaconfigfile")));

    base::RunLoop run_loop;
    hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager_->OnHintsComponentAvailable(info);
    run_loop.Run();
  }

  void ProcessHintsComponentInfoWithBadConfig(const std::string& version) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("badconfig.pb")));
    ASSERT_TRUE(base::WriteFile(info.path, "garbage"));

    hints_manager_->OnHintsComponentAvailable(info);
    RunUntilIdle();
  }

  void ProcessHints(const proto::Configuration& config,
                    const std::string& version,
                    bool should_wait = true) {
    HintsComponentInfo info(
        base::Version(version),
        temp_dir().Append(FILE_PATH_LITERAL("somefile.pb")));
    ASSERT_NO_FATAL_FAILURE(WriteConfigToFile(config, info.path));

    base::RunLoop run_loop;
    if (should_wait) {
      hints_manager_->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    }
    hints_manager_->OnHintsComponentAvailable(info);
    if (should_wait) {
      run_loop.Run();
    }
  }

  void InitializeWithDefaultConfig(const std::string& version,
                                   bool should_wait = true) {
    proto::Configuration config;
    proto::Hint* hint1 = config.add_hints();
    hint1->set_key("somedomain.org");
    hint1->set_key_representation(proto::HOST);
    hint1->set_version("someversion");
    proto::PageHint* page_hint1 = hint1->add_page_hints();
    page_hint1->set_page_pattern("/news/");
    proto::Optimization* default_opt =
        page_hint1->add_allowlisted_optimizations();
    default_opt->set_optimization_type(proto::NOSCRIPT);
    // Add another hint so somedomain.org hint is not in-memory initially.
    proto::Hint* hint2 = config.add_hints();
    hint2->set_key("somedomain2.org");
    hint2->set_key_representation(proto::HOST);
    hint2->set_version("someversion");
    proto::Optimization* opt = hint2->add_allowlisted_optimizations();
    opt->set_optimization_type(proto::NOSCRIPT);

    ProcessHints(config, version, should_wait);
  }

  void MoveClockForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
    RunUntilIdle();
  }

  // Creates navigation data for a navigation to |url| with registered
  // |optimization_types|.
  std::unique_ptr<OptimizationGuideNavigationData> CreateTestNavigationData(
      const GURL& url,
      const std::vector<proto::OptimizationType>& optimization_types) {
    auto navigation_data = std::make_unique<OptimizationGuideNavigationData>(
        /*navigation_id=*/1, /*navigation_start*/ base::TimeTicks::Now());
    navigation_data->set_navigation_url(url);
    navigation_data->set_registered_optimization_types(optimization_types);
    return navigation_data;
  }

  void CallOnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data,
      base::OnceClosure callback) {
    hints_manager()->OnNavigationStartOrRedirect(navigation_data,
                                                 std::move(callback));
  }

  HintsManager* hints_manager() const { return hints_manager_.get(); }

  int32_t num_batch_update_hints_fetches_initiated() const {
    return hints_manager()->num_batch_update_hints_fetches_initiated();
  }

  GURL url_with_hints() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_with_url_keyed_hint() const {
    return GURL("https://somedomain.org/news/whatever");
  }

  GURL url_without_hints() const {
    return GURL("https://url_without_hints.org/");
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

  PrefService* pref_service() const { return pref_service_.get(); }

  FakeTabUrlProvider* tab_url_provider() const {
    return tab_url_provider_.get();
  }

  FakeTopHostProvider* top_host_provider() const {
    return top_host_provider_.get();
  }

  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  OptimizationGuideLogger optimization_guide_logger_;
  std::unique_ptr<HintsManager> hints_manager_;

 private:
  void WriteConfigToFile(const proto::Configuration& config,
                         const base::FilePath& filePath) {
    std::string serialized_config;
    ASSERT_TRUE(config.SerializeToString(&serialized_config));
    ASSERT_TRUE(base::WriteFile(filePath, serialized_config));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<OptimizationGuideStore> hint_store_;
  std::unique_ptr<FakeTabUrlProvider> tab_url_provider_;
  std::unique_ptr<FakeTopHostProvider> top_host_provider_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(HintsManagerTest, ProcessHintsWithValidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(proto::NOSCRIPT);
  BloomFilter bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                           kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::PERFORMANCE_HINTS, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kHintsProtoOverrideSwitch, encoded_config);
  CreateHintsManager(/*top_host_provider=*/nullptr);
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                      ProcessHintsComponentResult::kSuccess, 1);
  // However, we still expect the local histogram for the hints being updated to
  // be recorded.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.UpdateComponentHints.Result", true, 1);

  // Bloom filters passed via command line are processed on the background
  // thread so make sure everything has finished before checking if it has been
  // loaded.
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
      proto::LITE_PAGE_REDIRECT));
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
      proto::PERFORMANCE_HINTS));
  const base::DictValue& previous_opt_types_with_filter =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(2u, previous_opt_types_with_filter.size());
  EXPECT_TRUE(previous_opt_types_with_filter.contains(
      optimization_guide::proto::OptimizationType_Name(
          proto::LITE_PAGE_REDIRECT)));
  EXPECT_TRUE(previous_opt_types_with_filter.contains(
      optimization_guide::proto::OptimizationType_Name(
          proto::PERFORMANCE_HINTS)));

  // Now register a new type with an allowlist that has not yet been loaded.
  hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});
  RunUntilIdle();

  EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
      proto::PERFORMANCE_HINTS));
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidCommandLineOverride) {
  base::HistogramTester histogram_tester;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      kHintsProtoOverrideSwitch, "this-is-not-a-proto");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // The below histogram should not be recorded since hints weren't coming
  // directly from the component.
  histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult", 0);
  // We also do not expect to update the component hints with bad hints either.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.UpdateComponentHints.Result", 0);
}

TEST_F(HintsManagerTest,
       ProcessHintsWithCommandLineOverrideShouldNotBeOverriddenByNewComponent) {
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("noscript_default_2g");
  proto::Optimization* optimization =
      page_hint->add_allowlisted_optimizations();
  optimization->set_optimization_type(proto::NOSCRIPT);

  std::string encoded_config;
  config.SerializeToString(&encoded_config);
  encoded_config = base::Base64Encode(encoded_config);

  {
    base::HistogramTester histogram_tester;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        kHintsProtoOverrideSwitch, encoded_config);
    CreateHintsManager(/*top_host_provider=*/nullptr);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.UpdateComponentHints.Result", true, 1);
  }

  // Test that a new component coming in does not update the component hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0.0");
    // The below histograms should not be recorded since component hints
    // processing is disabled.
    histogram_tester.ExpectTotalCount("OptimizationGuide.ProcessHintsResult",
                                      0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.UpdateComponentHints.Result", 0);
  }
}

TEST_F(HintsManagerTest, ParseTwoConfigVersions) {
  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("/news/");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::RESOURCE_LOADING);

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This should also update the hints.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest, ParseInvalidConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("1.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  {
    base::HistogramTester histogram_tester;
    ProcessHintsComponentInfoWithBadConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kFailedInvalidConfiguration, 1);
  }
}

TEST_F(HintsManagerTest, ComponentProcessingWhileShutdown) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("10.0.0.0", /*should_wait=*/false);
  hints_manager()->Shutdown();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessingComponentAtShutdown", true, 1);

  EXPECT_TRUE(
      pref_service()->GetString(prefs::kPendingHintsProcessingVersion).empty());
}

TEST_F(HintsManagerTest, ParseOlderConfigVersions) {
  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("10.0.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as an older version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0.0");
    // If we have already parsed a version later than this version, we expect
    // for the hints to not be updated.
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kSkippedProcessingHints, 1);
  }
}

TEST_F(HintsManagerTest, ParseDuplicateConfigVersions) {
  const std::string version = "3.0.0.0";

  // Test the first time parsing the config.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }

  // Test the second time parsing the config. This will be treated by the cache
  // as a duplicate version.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig(version);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ProcessHintsResult",
        ProcessHintsComponentResult::kSkippedProcessingHints, 1);
  }
}

TEST_F(HintsManagerTest, ComponentInfoDidNotContainConfig) {
  base::HistogramTester histogram_tester;
  ProcessInvalidHintsComponentInfo("1.0.0.0");
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ProcessHintsResult",
      ProcessHintsComponentResult::kFailedReadingFile, 1);
}

TEST_F(HintsManagerTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest, ProcessHintsWithInvalidPref) {
  // Create pref file with invalid version.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "bad-2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config is processed with pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}

TEST_F(HintsManagerTest, ProcessHintsUpdatePreviousOptTypesWithFilter) {
  proto::Configuration config_one;
  BloomFilter bloom_filter_one(kDefaultHostBloomFilterNumHashFunctions,
                               kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter_one);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter_one,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config_one);
  AddBloomFilterToConfig(proto::PERFORMANCE_HINTS, bloom_filter_one,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config_one);
  ProcessHints(config_one, "1.0.0.0");

  const base::DictValue& dic_one =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(2u, dic_one.size());
  EXPECT_TRUE(dic_one.contains(optimization_guide::proto::OptimizationType_Name(
      proto::LITE_PAGE_REDIRECT)));
  EXPECT_TRUE(dic_one.contains(optimization_guide::proto::OptimizationType_Name(
      proto::PERFORMANCE_HINTS)));

  proto::Configuration config_two;
  BloomFilter bloom_filter_two(kDefaultHostBloomFilterNumHashFunctions,
                               kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter_two);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter_two,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config_two);
  ProcessHints(config_two, "2.0.0.0");

  const base::DictValue& dic_two =
      pref_service()->GetDict(prefs::kPreviousOptimizationTypesWithFilter);
  EXPECT_EQ(1u, dic_two.size());
  EXPECT_TRUE(dic_two.contains(optimization_guide::proto::OptimizationType_Name(
      proto::LITE_PAGE_REDIRECT)));
}

TEST_F(HintsManagerTest,
       OnNavigationStartOrRedirectNoTypesRegisteredShouldNotLoadHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");

  auto navigation_data = CreateTestNavigationData(url_with_hints(), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectWithHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data = CreateTestNavigationData(url_with_hints(), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      true, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHint) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data =
      CreateTestNavigationData(GURL("https://notinhints.com"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample("OptimizationGuide.LoadedHint.Result",
                                      false, 1);
}

TEST_F(HintsManagerTest, OnNavigationStartOrRedirectNoHost) {
  base::HistogramTester histogram_tester;
  InitializeWithDefaultConfig("3.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  auto navigation_data = CreateTestNavigationData(GURL("blargh"), {});

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectTotalCount("OptimizationGuide.LoadedHint.Result", 0);
}

TEST_F(HintsManagerTest, OptimizationFiltersAreOnlyLoadedIfTypeIsRegistered) {
  proto::Configuration config;
  BloomFilter bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                           kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::NOSCRIPT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::DEFER_ALL_SCRIPT, bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);

  {
    base::HistogramTester histogram_tester;

    ProcessHints(config, "1.0.0.0");

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Now register the optimization type and see that it is loaded.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        proto::LITE_PAGE_REDIRECT));
    EXPECT_FALSE(
        hints_manager()->HasLoadedOptimizationBlocklist(proto::NOSCRIPT));
    EXPECT_FALSE(hints_manager()->HasLoadedOptimizationAllowlist(
        proto::DEFER_ALL_SCRIPT));
  }

  // Re-registering the same optimization type does not re-load the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization type without a filter does not trigger a
  // reload of the filter.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes({proto::PERFORMANCE_HINTS});
    run_loop.Run();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript", 0);
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript", 0);
  }

  // Registering a new optimization types with filters does trigger a
  // reload of the filters.
  {
    base::HistogramTester histogram_tester;

    base::RunLoop run_loop;
    hints_manager()->ListenForNextUpdateForTesting(run_loop.QuitClosure());
    hints_manager()->RegisterOptimizationTypes(
        {proto::NOSCRIPT, proto::DEFER_ALL_SCRIPT});
    run_loop.Run();

    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.NoScript",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        OptimizationFilterStatus::kFoundServerFilterConfig, 1);
    histogram_tester.ExpectBucketCount(
        "OptimizationGuide.OptimizationFilterStatus.DeferAllScript",
        OptimizationFilterStatus::kCreatedServerFilter, 1);
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationBlocklist(
        proto::LITE_PAGE_REDIRECT));
    EXPECT_TRUE(
        hints_manager()->HasLoadedOptimizationBlocklist(proto::NOSCRIPT));
    EXPECT_TRUE(hints_manager()->HasLoadedOptimizationAllowlist(
        proto::DEFER_ALL_SCRIPT));
  }
}

TEST_F(HintsManagerTest, OptimizationFiltersOnlyLoadOncePerType) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  // Make sure it will only load one of an allowlist or a blocklist.
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  // We found 2 LPR blocklists: parsed one and duped the other.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFoundServerFilterConfig, 3);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kCreatedServerFilter, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFailedServerFilterDuplicateConfig, 2);
}

TEST_F(HintsManagerTest, InvalidOptimizationFilterNotLoaded) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  base::HistogramTester histogram_tester;

  int too_many_bits = kMaxServerBloomFilterBits + 1;

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     too_many_bits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions, too_many_bits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFoundServerFilterConfig, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationFilterStatus.LitePageRedirect",
      OptimizationFilterStatus::kFailedServerFilterTooBig, 1);
  EXPECT_FALSE(hints_manager()->HasLoadedOptimizationBlocklist(
      proto::LITE_PAGE_REDIRECT));
}

TEST_F(HintsManagerTest, CanApplyOptimizationUrlWithNoHost) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("urlwithnohost"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  // Make sure decisions are logged correctly.
  EXPECT_EQ(OptimizationTypeDecision::kInvalidURL, optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet_ComponentReady) {
  // Simulate a situation where the component is ready, but the filter has not
  // been loaded yet.
  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");
  // Append the switch for processing hints to force the filter to not get
  // loaded.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kHintsProtoOverrideSwitch);

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);

  // Run until idle to ensure we don't crash because the test object has gone
  // away.
  RunUntilIdle();
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasFilterForTypeButNotLoadedYet_ComponentNotReady) {
  // Simulate a situation where the component not ready, but we know from
  // previous sessions that LITE_PAGE_REDIRECT type has a filter.
  ScopedDictPrefUpdate previous_opt_types_with_filter(
      pref_service(), prefs::kPreviousOptimizationTypesWithFilter);
  previous_opt_types_with_filter->Set(
      optimization_guide::proto::OptimizationType_Name(
          proto::LITE_PAGE_REDIRECT),
      true);

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInAllowlist) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://m.host.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlInBlocklist) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://m.host.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInAllowlistFilter) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter allowlist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&allowlist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, allowlist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/true, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationHasLoadedFilterForTypeUrlNotInBlocklistFilter) {
  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(GURL("https://whatever.com/123"),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationOptimizationTypeAllowlistedAtTopLevel) {
  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("somedomain.org");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::Optimization* opt1 = hint1->add_allowlisted_optimizations();
  opt1->set_optimization_type(proto::RESOURCE_LOADING);
  ProcessHints(config, "1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::RESOURCE_LOADING});

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::RESOURCE_LOADING});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::RESOURCE_LOADING,
                                            &optimization_metadata);
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasPageHintButNoMatchingOptType) {
  InitializeWithDefaultConfig("1.0.0.0");
  hints_manager()->RegisterOptimizationTypes({proto::DEFER_ALL_SCRIPT});

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::DEFER_ALL_SCRIPT});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::DEFER_ALL_SCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAndPopulatesLoadingPredictorMetadata) {
  hints_manager()->RegisterOptimizationTypes({proto::LOADING_PREDICTOR});
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  hint->set_version("someversion");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(proto::LOADING_PREDICTOR);
  opt->mutable_loading_predictor_metadata()->add_subresources()->set_url(
      "https://resource.com/");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LOADING_PREDICTOR,
                                            &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(optimization_metadata.loading_predictor_metadata().has_value());
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationAndPopulatesAnyMetadata) {
  hints_manager()->RegisterOptimizationTypes({proto::LOADING_PREDICTOR});
  proto::Configuration config;
  proto::Hint* hint = config.add_hints();
  hint->set_key("somedomain.org");
  hint->set_key_representation(proto::HOST);
  hint->set_version("someversion");
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("/news/");
  proto::Optimization* opt = page_hint->add_allowlisted_optimizations();
  opt->set_optimization_type(proto::LOADING_PREDICTOR);
  proto::LoadingPredictorMetadata lp_metadata;
  lp_metadata.add_subresources()->set_url("https://resource.com/");
  lp_metadata.SerializeToString(opt->mutable_any_metadata()->mutable_value());
  opt->mutable_any_metadata()->set_type_url(
      "type.googleapis.com/com.foo.LoadingPredictorMetadata");

  ProcessHints(config, "1.0.0.0");

  auto navigation_data =
      CreateTestNavigationData(url_with_hints(), {proto::LOADING_PREDICTOR});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LOADING_PREDICTOR,
                                            &optimization_metadata);
  // Make sure loading predictor metadata is populated.
  EXPECT_TRUE(
      optimization_metadata.ParsedMetadata<proto::LoadingPredictorMetadata>()
          .has_value());
  EXPECT_EQ(OptimizationTypeDecision::kAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationNoMatchingPageHint) {
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});

  auto navigation_data =
      CreateTestNavigationData(GURL("https://somedomain.org/nomatch"), {});
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::NOSCRIPT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByHint,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationNoHintForNavigationMetadataClearedAnyway) {
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      GURL("https://nohint.com"), {proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});
  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::NOSCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kNoHintAvailable,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationHasHintInCacheButNotLoaded) {
  InitializeWithDefaultConfig("1.0.0.0");

  hints_manager()->RegisterOptimizationTypes({proto::NOSCRIPT});
  OptimizationMetadata optimization_metadata;
  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(url_with_hints(), proto::NOSCRIPT,
                                            &optimization_metadata);

  EXPECT_EQ(OptimizationTypeDecision::kHadHintButNotLoadedInTime,
            optimization_type_decision);
}

TEST_F(HintsManagerTest, CanApplyOptimizationFilterTakesPrecedence) {
  auto navigation_data = CreateTestNavigationData(
      GURL("https://m.host.com/urlinfilterandhints"), {});

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("host.com");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://m.host.com");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::LITE_PAGE_REDIRECT);
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  // Make sure decision points logged correctly.
  EXPECT_EQ(OptimizationTypeDecision::kNotAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationFilterTakesPrecedenceMatchesFilter) {
  auto navigation_data =
      CreateTestNavigationData(GURL("https://notfiltered.com/whatever"),
                               {proto::COMPRESS_PUBLIC_IMAGES});

  hints_manager()->RegisterOptimizationTypes({proto::LITE_PAGE_REDIRECT});

  proto::Configuration config;
  proto::Hint* hint1 = config.add_hints();
  hint1->set_key("notfiltered.com");
  hint1->set_key_representation(proto::HOST);
  hint1->set_version("someversion");
  proto::PageHint* page_hint1 = hint1->add_page_hints();
  page_hint1->set_page_pattern("https://notfiltered.com");
  proto::Optimization* optimization1 =
      page_hint1->add_allowlisted_optimizations();
  optimization1->set_optimization_type(proto::LITE_PAGE_REDIRECT);
  BloomFilter blocklist_bloom_filter(kDefaultHostBloomFilterNumHashFunctions,
                                     kDefaultHostBloomFilterNumBits);
  PopulateBloomFilterWithDefaultHost(&blocklist_bloom_filter);
  AddBloomFilterToConfig(proto::LITE_PAGE_REDIRECT, blocklist_bloom_filter,
                         kDefaultHostBloomFilterNumHashFunctions,
                         kDefaultHostBloomFilterNumBits,
                         /*is_allowlist=*/false, &config);
  ProcessHints(config, "1.0.0.0");

  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  OptimizationTypeDecision optimization_type_decision =
      hints_manager()->CanApplyOptimization(navigation_data->navigation_url(),
                                            proto::LITE_PAGE_REDIRECT,
                                            /*optimization_metadata=*/nullptr);

  EXPECT_EQ(OptimizationTypeDecision::kAllowedByOptimizationFilter,
            optimization_type_decision);
}

TEST_F(HintsManagerTest,
       CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetch) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_without_hints(), {proto::COMPRESS_PUBLIC_IMAGES});
  hints_manager()->CanApplyOptimizationAsync(
      url_without_hints(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNoHintAvailable, 1);
}

TEST_F(
    HintsManagerTest,
    CanApplyOptimizationAsyncReturnsRightAwayIfNotAllowedToFetchAndNotAllowlistedByAvailableHint) {
  base::HistogramTester histogram_tester;

  hints_manager()->RegisterOptimizationTypes({proto::COMPRESS_PUBLIC_IMAGES});
  InitializeWithDefaultConfig("1.0.0.0");

  auto navigation_data = CreateTestNavigationData(
      url_with_hints(), {proto::COMPRESS_PUBLIC_IMAGES});
  // Wait for hint to be loaded.
  base::RunLoop run_loop;
  CallOnNavigationStartOrRedirect(navigation_data.get(),
                                  run_loop.QuitClosure());
  run_loop.Run();

  hints_manager()->CanApplyOptimizationAsync(
      url_with_hints(), proto::COMPRESS_PUBLIC_IMAGES,
      base::BindOnce([](OptimizationGuideDecision decision,
                        const OptimizationMetadata& metadata) {
        EXPECT_EQ(OptimizationGuideDecision::kFalse, decision);
      }));
  RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ApplyDecision.CompressPublicImages",
      OptimizationTypeDecision::kNotAllowedByHint, 1);
}

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_Host) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.GetHost());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.GetHost()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.GetHost()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::HOST, {url.GetHost()});
  run_loop->Run();

  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.GetHost()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

TEST_F(HintsManagerTest, RemoveFetchedEntriesByHintKeys_URL) {
  int cache_duration_in_secs = 60;
  GURL url("https://host.com/r/cats");

  std::unique_ptr<proto::GetHintsResponse> get_hints_response =
      std::make_unique<proto::GetHintsResponse>();

  proto::Hint* hint = get_hints_response->add_hints();
  hint->set_key(url.spec());
  hint->set_key_representation(proto::FULL_URL);
  hint->mutable_max_cache_duration()->set_seconds(cache_duration_in_secs);
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->add_allowlisted_optimizations()->set_optimization_type(
      proto::PERFORMANCE_HINTS);
  page_hint->set_page_pattern("whatever/*");

  hint = get_hints_response->add_hints();
  hint->set_key_representation(proto::HOST);
  hint->set_key(url.GetHost());
  page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("anything/*");

  std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->hint_cache()->UpdateFetchedHints(
      std::move(get_hints_response), base::Time().Now(), {url.GetHost()}, {url},
      run_loop->QuitClosure());
  EXPECT_TRUE(hints_manager()->hint_cache()->HasHint(url.GetHost()));
  EXPECT_TRUE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));

  run_loop = std::make_unique<base::RunLoop>();
  hints_manager()->RemoveFetchedEntriesByHintKeys(
      run_loop->QuitClosure(), proto::KeyRepresentation::FULL_URL,
      {url.spec()});
  run_loop->Run();

  // Both the host and url entries should have been removed to support upgrading
  // hint keys from HOST to FULL_URL.
  EXPECT_FALSE(hints_manager()->hint_cache()->HasHint(url.GetHost()));
  EXPECT_FALSE(hints_manager()->hint_cache()->HasURLKeyedEntryForURL(url));
}

using HintsManagerComponentSkipProcessingTest = HintsManagerTest;

TEST_F(HintsManagerComponentSkipProcessingTest, ProcessHintsWithExistingPref) {
  // Write hints processing pref for version 2.0.0.
  pref_service()->SetString(prefs::kPendingHintsProcessingVersion, "2.0.0");
  CreateHintsManager(/*top_host_provider=*/nullptr);

  // Verify config still processed even though pref is existing.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("2.0.0");
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
    // If it processed correctly, it should clear the pref.
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
  }

  // Now verify config is processed for different version and pref cleared.
  {
    base::HistogramTester histogram_tester;
    InitializeWithDefaultConfig("3.0.0");
    EXPECT_TRUE(pref_service()
                    ->GetString(prefs::kPendingHintsProcessingVersion)
                    .empty());
    histogram_tester.ExpectUniqueSample("OptimizationGuide.ProcessHintsResult",
                                        ProcessHintsComponentResult::kSuccess,
                                        1);
  }
}
}  // namespace optimization_guide
