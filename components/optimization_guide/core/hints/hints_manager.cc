// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/hints/hints_manager.h"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/access_token_helper.h"
#include "components/optimization_guide/core/filters/bloom_filter.h"
#include "components/optimization_guide/core/filters/hints_component_util.h"
#include "components/optimization_guide/core/filters/optimization_filter.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_update_listener.h"
#include "components/optimization_guide/core/hints/hint_cache.h"
#include "components/optimization_guide/core/hints/hints_processing_util.h"
#include "components/optimization_guide/core/hints/insertion_ordered_set.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_guide_navigation_data.h"
#include "components/optimization_guide/core/hints/optimization_guide_store.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/core/hints/tab_url_provider.h"
#include "components/optimization_guide/core/hints/top_host_provider.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace optimization_guide {

std::unique_ptr<proto::Configuration>
ParseComponentConfigFromCommandLine() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kHintsProtoOverrideSwitch))
    return nullptr;

  std::string b64_pb = cmd_line->GetSwitchValueASCII(kHintsProtoOverrideSwitch);

  std::string binary_pb;
  if (!base::Base64Decode(b64_pb, &binary_pb)) {
    LOG(ERROR) << "Invalid base64 encoding of the Hints Proto Override";
    return nullptr;
  }

  auto proto_configuration = std::make_unique<proto::Configuration>();
  if (!proto_configuration->ParseFromString(binary_pb)) {
    LOG(ERROR) << "Invalid proto provided to the Hints Proto Override";
    return nullptr;
  }

  return proto_configuration;
}

const char kLoadedHintLocalHistogramString[] =
    "OptimizationGuide.LoadedHint.Result";

BASE_FEATURE(kHintsBatchUpdateForActiveTabsAndTopHosts,
             "OptimizationGuideHintsBatchUpdateForActiveTabsAndTopHosts",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif


namespace {

bool IsHintComponentProcessingDisabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      kHintsProtoOverrideSwitch);
}

bool ShouldPurgeOptimizationGuideStoreOnStartup() {
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  return cmd_line->HasSwitch(kHintsProtoOverrideSwitch) ||
         cmd_line->HasSwitch(kPurgeHintsStoreSwitch);
}

// The max duration since the time a tab has to be shown to be
// considered active for a hints refresh.
constexpr base::TimeDelta kActiveTabsStalenessTolerace = base::Days(90);

// The component version used with a manual config. This ensures that any hint
// component received from the Optimization Hints component on a subsequent
// startup will have a newer version than it.
constexpr char kManualConfigComponentVersion[] = "0.0.0";

void MaybeRunUpdateClosure(base::OnceClosure update_closure) {
  if (update_closure) {
    std::move(update_closure).Run();
  }
}

std::optional<base::Version>
GetPendingOptimizationHintsComponentVersionFromPref(PrefService* pref_service) {
  const std::string previous_attempted_version_string =
      pref_service->GetString(prefs::kPendingHintsProcessingVersion);
  if (!previous_attempted_version_string.empty()) {
    const base::Version previous_attempted_version =
        base::Version(previous_attempted_version_string);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in hints processing pref";
      // Clear pref for fresh start next time.
      pref_service->ClearPref(prefs::kPendingHintsProcessingVersion);
      return std::nullopt;
    }
    return std::make_optional(previous_attempted_version);
  }
  return std::nullopt;
}

// Returns whether |optimization_type| is allowlisted by |optimizations|. If
// it is allowlisted, this will return true and |optimization_metadata| will be
// populated with the metadata provided by the hint, if applicable. If
// |page_hint| is not provided or |optimization_type| is not allowlisted, this
// will return false.
bool IsOptimizationTypeAllowed(const google::protobuf::RepeatedPtrField<
                                   proto::Optimization>& optimizations,
                               proto::OptimizationType optimization_type,
                               OptimizationMetadata* optimization_metadata) {
  for (const auto& optimization : optimizations) {
    if (optimization_type != optimization.optimization_type()) {
      continue;
    }

    // We found an optimization that can be applied. Populate optimization
    // metadata if applicable and return.
    if (optimization_metadata) {
      switch (optimization.metadata_case()) {
        case proto::Optimization::kLoadingPredictorMetadata:
          optimization_metadata->set_loading_predictor_metadata(
              optimization.loading_predictor_metadata());
          break;
        case proto::Optimization::kAnyMetadata:
          optimization_metadata->set_any_metadata(optimization.any_metadata());
          break;
        case proto::Optimization::METADATA_NOT_SET:
          // Some optimization types do not have metadata, make sure we do not
          // DCHECK.
          break;
      }
    }
    return true;
  }

  return false;
}

// Util class for recording whether a hints fetch race against the current
// navigation was attempted. The result is recorded when it goes out of scope
// and its destructor is called.
class ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder {
 public:
  explicit ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder(
      OptimizationGuideNavigationData* navigation_data)
      : race_attempt_status_(RaceNavigationFetchAttemptStatus::kUnknown),
        navigation_data_(navigation_data) {}

  ~ScopedHintsManagerRaceNavigationHintsFetchAttemptRecorder() {
    DCHECK_NE(race_attempt_status_, RaceNavigationFetchAttemptStatus::kUnknown);
    DCHECK_NE(
        race_attempt_status_,
        RaceNavigationFetchAttemptStatus::
            kDeprecatedRaceNavigationFetchNotAttemptedTooManyConcurrentFetches);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.HintsManager.RaceNavigationFetchAttemptStatus",
        race_attempt_status_);
    if (navigation_data_) {
      navigation_data_->set_hints_fetch_attempt_status(race_attempt_status_);
    }
  }

 private:
  RaceNavigationFetchAttemptStatus race_attempt_status_;
  raw_ptr<OptimizationGuideNavigationData> navigation_data_;
};

// Returns true if the optimization type should be ignored when is newly
// registered as the optimization type is likely launched.
bool ShouldIgnoreNewlyRegisteredOptimizationType(
    proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case proto::NOSCRIPT:
    case proto::RESOURCE_LOADING:
    case proto::LITE_PAGE_REDIRECT:
    case proto::DEFER_ALL_SCRIPT:
      return true;
    default:
      return false;
  }
}

class ScopedCanApplyOptimizationLogger {
 public:
  ScopedCanApplyOptimizationLogger(
      proto::OptimizationType opt_type,
      GURL url,
      OptimizationGuideLogger* optimization_guide_logger)
      : decision_(OptimizationGuideDecision::kUnknown),
        type_decision_(OptimizationTypeDecision::kUnknown),
        opt_type_(opt_type),
        has_metadata_(false),
        url_(url),
        optimization_guide_logger_(optimization_guide_logger) {}

  ~ScopedCanApplyOptimizationLogger() {
    if (!optimization_guide_logger_->ShouldEnableDebugLogs()) {
      return;
    }
    DCHECK_NE(type_decision_, OptimizationTypeDecision::kUnknown);
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::HINTS,
        optimization_guide_logger_)
        << "CanApplyOptimization: "
        << GetStringNameForOptimizationType(opt_type_)
        << "\nqueried on: " << url_
        << "\nDecision: " << GetStringForOptimizationGuideDecision(decision_)
        << "\nTypeDecision: "
        << base::NumberToString(static_cast<int>(type_decision_))
        << "\nHas Metadata: " << (has_metadata_ ? "True" : "False");
  }

  void set_has_metadata() { has_metadata_ = true; }

  void set_type_decision(OptimizationTypeDecision type_decision) {
    type_decision_ = type_decision;
    decision_ =
        HintsManager::GetOptimizationGuideDecisionFromOptimizationTypeDecision(
            type_decision_);
  }

 private:
  OptimizationGuideDecision decision_;
  OptimizationTypeDecision type_decision_;
  proto::OptimizationType opt_type_;
  bool has_metadata_;
  GURL url_;

  // Not owned. Guaranteed to outlive |this| scoped object.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;
};

// Reads component file and parses it into a Configuration proto. Should not be
// called on the UI thread.
std::unique_ptr<proto::Configuration> ReadComponentFile(
    const HintsComponentInfo& info) {
  ProcessHintsComponentResult out_result;
  std::unique_ptr<proto::Configuration> config =
      ProcessHintsComponent(info, &out_result);
  if (!config) {
    RecordProcessHintsComponentResult(out_result);
    return nullptr;
  }

  // Do not record the process hints component result for success cases until
  // we processed all of the hints and filters in it.
  return config;
}

// Logs information that will be requested from the remote Optimization Guide
// service.
// Returns whether the context should be used to seed the hint cache.
void RecordOptimizationFilterStatus(proto::OptimizationType optimization_type,
                                    OptimizationFilterStatus status) {
  base::UmaHistogramExactLinear(
      base::StrCat({"OptimizationGuide.OptimizationFilterStatus.",
                    GetStringNameForOptimizationType(optimization_type)}),
      static_cast<int>(status),
      static_cast<int>(OptimizationFilterStatus::kMaxValue));
}

}  // namespace

HintsManager::HintsManager(
    bool is_off_the_record,
    const std::string& application_locale,
    PrefService* pref_service,
    base::WeakPtr<OptimizationGuideStore> hint_store,
    TopHostProvider* top_host_provider,
    TabUrlProvider* tab_url_provider,
    std::unique_ptr<PushNotificationManager> push_notification_manager,
    signin::IdentityManager* identity_manager,
    OptimizationGuideLogger* optimization_guide_logger)
    : optimization_guide_logger_(optimization_guide_logger),
      failed_component_version_(
          GetPendingOptimizationHintsComponentVersionFromPref(pref_service)),
      is_off_the_record_(is_off_the_record),
      application_locale_(application_locale),
      pref_service_(pref_service),
      hint_cache_(
          std::make_unique<HintCache>(hint_store,
                                      features::MaxHostKeyedHintCacheSize())),
      top_host_provider_(top_host_provider),
      tab_url_provider_(tab_url_provider),
      push_notification_manager_(std::move(push_notification_manager)),
      identity_manager_(identity_manager),
      clock_(base::DefaultClock::GetInstance()),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      allowed_optimization_types_for_proactive_personalization_(
          features::GetAllowedOptimizationTypesForProactivePersonalization()) {
  if (push_notification_manager_) {
    push_notification_manager_->SetDelegate(this);
  }

  // Register as an observer to get updates for the component. This is
  // needed as a signal during testing.
  OptimizationHintsComponentUpdateListener::GetInstance()->AddObserver(this);

  hint_cache_->Initialize(
      ShouldPurgeOptimizationGuideStoreOnStartup(),
      base::BindOnce(&HintsManager::OnHintCacheInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}
HintsManager::~HintsManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void HintsManager::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OptimizationHintsComponentUpdateListener::GetInstance()->RemoveObserver(this);

  base::UmaHistogramBoolean(
      "OptimizationGuide.ProcessingComponentAtShutdown",
      currently_processing_component_version_.has_value());
  if (currently_processing_component_version_) {
    // If we are currently processing the component and we are asked to shut
    // down, we should clear the pref since the function to clear the pref will
    // not run after shut down and we will think that we failed to process the
    // component due to a crash.
    pref_service_->ClearPref(prefs::kPendingHintsProcessingVersion);
  }
}

// static
OptimizationGuideDecision
HintsManager::GetOptimizationGuideDecisionFromOptimizationTypeDecision(
    OptimizationTypeDecision optimization_type_decision) {
  switch (optimization_type_decision) {
    case OptimizationTypeDecision::kAllowedByOptimizationFilter:
    case OptimizationTypeDecision::kAllowedByHint:
      return OptimizationGuideDecision::kTrue;
    case OptimizationTypeDecision::kUnknown:
    case OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime:
    case OptimizationTypeDecision::kHadHintButNotLoadedInTime:
    case OptimizationTypeDecision::kHintFetchStartedButNotAvailableInTime:
    case OptimizationTypeDecision::kDeciderNotInitialized:
      return OptimizationGuideDecision::kUnknown;
    case OptimizationTypeDecision::kNotAllowedByHint:
    case OptimizationTypeDecision::kNoMatchingPageHint:
    case OptimizationTypeDecision::kNoHintAvailable:
    case OptimizationTypeDecision::kNotAllowedByOptimizationFilter:
    case OptimizationTypeDecision::kInvalidURL:
    case OptimizationTypeDecision::kRequestedUnregisteredType:
      return OptimizationGuideDecision::kFalse;
  }
}

void HintsManager::OnHintsComponentAvailable(const HintsComponentInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (currently_processing_component_version_ &&
      *currently_processing_component_version_ == info.version) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::HINTS,
        optimization_guide_logger_)
        << "Already in the middle of processing OptimizationHints component "
           "version: "
        << info.version.GetString();
    return;
  }

  OPTIMIZATION_GUIDE_LOGGER(optimization_guide_common::mojom::LogSource::HINTS,
                            optimization_guide_logger_)
      << "Received OptimizationHints component version: "
      << info.version.GetString();

  // Check for if hint component is disabled. This check is needed because the
  // optimization guide still registers with the service as an observer for
  // components as a signal during testing.
  if (IsHintComponentProcessingDisabled()) {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
    return;
  }

  // Write version that we are currently processing to prefs.
  pref_service_->SetString(prefs::kPendingHintsProcessingVersion,
                           info.version.GetString());

  std::unique_ptr<StoreUpdateData> update_data =
      is_off_the_record_
          ? nullptr
          : hint_cache_->MaybeCreateUpdateDataForComponentHints(info.version);

  // Processes the hints from the newly available component on a background
  // thread, providing a StoreUpdateData for component update from the hint
  // cache, so that each hint within the component can be moved into it. In the
  // case where the component's version is not newer than the optimization guide
  // store's component version, StoreUpdateData will be a nullptr and hint
  // processing will be skipped.
  // base::Unretained(this) is safe since |this| owns |background_task_runner_|
  // and the callback will be canceled if destroyed.
  currently_processing_component_version_ = info.version;
  OPTIMIZATION_GUIDE_LOGGER(optimization_guide_common::mojom::LogSource::HINTS,
                            optimization_guide_logger_)
      << "Processing OptimizationHints component version: "
      << currently_processing_component_version_->GetString();
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadComponentFile, info),
      base::BindOnce(&HintsManager::UpdateComponentHints,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_update_closure_), std::move(update_data)));

  // Only replace hints component info if it is not the same - otherwise we will
  // destruct the object and it will be invalid later.
  if (!hints_component_info_ ||
      hints_component_info_->version.CompareTo(info.version) != 0) {
    hints_component_info_.emplace(info.version, info.path);
  }
}

void HintsManager::ProcessOptimizationFilters(
    const google::protobuf::RepeatedPtrField<proto::OptimizationFilter>&
        allowlist_optimization_filters,
    const google::protobuf::RepeatedPtrField<proto::OptimizationFilter>&
        blocklist_optimization_filters) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  optimization_types_with_filter_.clear();
  allowlist_optimization_filters_.clear();
  blocklist_optimization_filters_.clear();
  ProcessOptimizationFilterSet(allowlist_optimization_filters,
                               /*is_allowlist=*/true);
  ProcessOptimizationFilterSet(blocklist_optimization_filters,
                               /*is_allowlist=*/false);

  ScopedDictPrefUpdate previous_opt_types_with_filter(
      pref_service_, prefs::kPreviousOptimizationTypesWithFilter);
  previous_opt_types_with_filter->clear();
  for (auto optimization_type : optimization_types_with_filter_) {
    previous_opt_types_with_filter->Set(
        optimization_guide::proto::OptimizationType_Name(optimization_type),
        true);
  }
}

void HintsManager::ProcessOptimizationFilterSet(
    const google::protobuf::RepeatedPtrField<proto::OptimizationFilter>&
        filters,
    bool is_allowlist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (const auto& filter : filters) {
    if (filter.optimization_type() != proto::TYPE_UNSPECIFIED) {
      optimization_types_with_filter_.insert(filter.optimization_type());
    }

    // Do not put anything in memory that we don't have registered.
    if (registered_optimization_types_.find(filter.optimization_type()) ==
        registered_optimization_types_.end()) {
      continue;
    }

    RecordOptimizationFilterStatus(
        filter.optimization_type(),
        OptimizationFilterStatus::kFoundServerFilterConfig);

    // Do not parse duplicate optimization filters.
    if (allowlist_optimization_filters_.find(filter.optimization_type()) !=
            allowlist_optimization_filters_.end() ||
        blocklist_optimization_filters_.find(filter.optimization_type()) !=
            blocklist_optimization_filters_.end()) {
      RecordOptimizationFilterStatus(
          filter.optimization_type(),
          OptimizationFilterStatus::kFailedServerFilterDuplicateConfig);
      continue;
    }

    // Parse optimization filter.
    OptimizationFilterStatus status;
    std::unique_ptr<OptimizationFilter> optimization_filter =
        ProcessOptimizationFilter(filter, &status);
    if (optimization_filter) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_)
          << "Loaded optimization filter for "
          << GetStringNameForOptimizationType(filter.optimization_type());
      if (is_allowlist) {
        allowlist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      } else {
        blocklist_optimization_filters_.insert(
            {filter.optimization_type(), std::move(optimization_filter)});
      }
    }
    RecordOptimizationFilterStatus(filter.optimization_type(), status);
  }
}

void HintsManager::OnHintCacheInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::HINTS,
        optimization_guide_logger_)
        << "Hint cache initialized";
  }

  if (push_notification_manager_) {
    push_notification_manager_->OnDelegateReady();
  }

  // Check if there is a valid hint proto given on the command line first. We
  // don't normally expect one, but if one is provided then use that and do not
  // register as an observer as the opt_guide service.
  std::unique_ptr<proto::Configuration> manual_config =
      ParseComponentConfigFromCommandLine();
  if (manual_config) {
    std::unique_ptr<StoreUpdateData> update_data =
        is_off_the_record_
            ? nullptr
            : hint_cache_->MaybeCreateUpdateDataForComponentHints(
                  base::Version(kManualConfigComponentVersion));
    // Allow |UpdateComponentHints| to block startup so that the first
    // navigation gets the hints when a command line hint proto is provided.
    UpdateComponentHints(base::DoNothing(), std::move(update_data),
                         std::move(manual_config));
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  // This is used as a signal for testing so that tests can push hints via the
  // component. Fixing the logic appropriately is a lot more work for something
  // we don't actually do in the wild.
  LOCAL_HISTOGRAM_BOOLEAN("OptimizationGuide.HintsManager.HintCacheInitialized",
                          true);
}

void HintsManager::UpdateComponentHints(
    base::OnceClosure update_closure,
    std::unique_ptr<StoreUpdateData> update_data,
    std::unique_ptr<proto::Configuration> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If we get here, the component file has been processed correctly and did not
  // crash the device.
  OPTIMIZATION_GUIDE_LOGGER(optimization_guide_common::mojom::LogSource::HINTS,
                            optimization_guide_logger_)
      << "Component successfully processed";
  currently_processing_component_version_ = std::nullopt;
  pref_service_->ClearPref(prefs::kPendingHintsProcessingVersion);

  if (!config) {
    MaybeRunUpdateClosure(std::move(update_closure));
    return;
  }

  ProcessOptimizationFilters(config->optimization_allowlists(),
                             config->optimization_blocklists());

  // Don't store hints in the store if it's off the record.
  if (update_data && !is_off_the_record_) {
    bool did_process_hints = hint_cache_->ProcessAndCacheHints(
        config->mutable_hints(), update_data.get());
    RecordProcessHintsComponentResult(
        did_process_hints ? ProcessHintsComponentResult::kSuccess
                          : ProcessHintsComponentResult::kProcessedNoHints);
  } else {
    RecordProcessHintsComponentResult(
        ProcessHintsComponentResult::kSkippedProcessingHints);
  }

  if (update_data) {
    hint_cache_->UpdateComponentHints(
        std::move(update_data),
        base::BindOnce(&HintsManager::OnComponentHintsUpdated,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(update_closure),
                       /* hints_updated=*/true));
  } else {
    OnComponentHintsUpdated(std::move(update_closure), /*hints_updated=*/false);
  }
}

void HintsManager::OnComponentHintsUpdated(base::OnceClosure update_closure,
                                           bool hints_updated) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record the result of updating the hints. This is used as a signal for the
  // hints being fully processed in testing.
  LOCAL_HISTOGRAM_BOOLEAN(kComponentHintsUpdatedResultHistogramString,
                          hints_updated);

  MaybeRunUpdateClosure(std::move(update_closure));
}

void HintsManager::ListenForNextUpdateForTesting(
    base::OnceClosure next_update_closure) {
  DCHECK(!next_update_closure_)
      << "Only one update closure is supported at a time";
  next_update_closure_ = std::move(next_update_closure);
}

void HintsManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

const std::vector<GURL> HintsManager::GetActiveTabURLsToRefresh() {
  if (!tab_url_provider_) {
    return {};
  }

  std::vector<GURL> active_tab_urls =
      tab_url_provider_->GetUrlsOfActiveTabs(kActiveTabsStalenessTolerace);

  std::set<GURL> urls_to_refresh;
  for (const auto& url : active_tab_urls) {
    if (!IsValidURLForURLKeyedHint(url)) {
      continue;
    }

    if (!hint_cache_->HasURLKeyedEntryForURL(url)) {
      urls_to_refresh.insert(url);
    }
  }
  return std::vector<GURL>(urls_to_refresh.begin(), urls_to_refresh.end());
}

// Helper method to check if any registered optimization type is allowed for
// proactive personalization
bool HintsManager::HasPersonalizableTypesRegistered() {
  return std::any_of(
      registered_optimization_types_.begin(),
      registered_optimization_types_.end(), [this](auto opt_type) {
        return allowed_optimization_types_for_proactive_personalization_.Has(
            opt_type);
      });
}

void HintsManager::LoadHintForURL(const GURL& url, base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url.has_host()) {
    MaybeRunUpdateClosure(std::move(callback));
    return;
  }

  LoadHintForHost(url.GetHost(), std::move(callback));
}

void HintsManager::LoadHintForHost(const std::string& host,
                                   base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  hint_cache_->LoadHint(host, base::BindOnce(&HintsManager::OnHintLoaded,
                                             weak_ptr_factory_.GetWeakPtr(),
                                             std::move(callback)));
}

void HintsManager::OnHintLoaded(base::OnceClosure callback,
                                const proto::Hint* loaded_hint) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Record the result of loading a hint. This is used as a signal for testing.
  LOCAL_HISTOGRAM_BOOLEAN(kLoadedHintLocalHistogramString, loaded_hint);

  // Run the callback now that the hint is loaded. This is used as a signal by
  // tests.
  MaybeRunUpdateClosure(std::move(callback));
}

void HintsManager::RegisterOptimizationTypes(
    const std::vector<proto::OptimizationType>& optimization_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  bool should_load_new_optimization_filter = false;

  ScopedDictPrefUpdate previously_registered_opt_types(
      pref_service_, prefs::kPreviouslyRegisteredOptimizationTypes);
  for (const auto optimization_type : optimization_types) {
    if (optimization_type == proto::TYPE_UNSPECIFIED) {
      continue;
    }

    if (registered_optimization_types_.find(optimization_type) !=
        registered_optimization_types_.end()) {
      continue;
    }
    registered_optimization_types_.insert(optimization_type);

    if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
      OPTIMIZATION_GUIDE_LOGGER(
          optimization_guide_common::mojom::LogSource::HINTS,
          optimization_guide_logger_)
          << "Registered new OptimizationType: "
          << GetStringNameForOptimizationType(optimization_type);
    }

    std::optional<double> value = previously_registered_opt_types->FindBool(
        proto::OptimizationType_Name(optimization_type));
    if (!value) {
      if (!is_off_the_record_ &&
          !ShouldIgnoreNewlyRegisteredOptimizationType(optimization_type) &&
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              kHintsProtoOverrideSwitch)) {
        should_clear_hints_for_new_type_ = true;
      }
      previously_registered_opt_types->Set(
          proto::OptimizationType_Name(optimization_type), true);
    }

    if (!should_load_new_optimization_filter) {
      if (optimization_types_with_filter_.find(optimization_type) !=
          optimization_types_with_filter_.end()) {
        should_load_new_optimization_filter = true;
      }
    }
  }

  // If the store is available, clear all hint state so newly registered types
  // can have their hints immediately included in hint fetches.
  if (hint_cache_->IsHintStoreAvailable() && should_clear_hints_for_new_type_) {
    ClearHostKeyedHints();
    should_clear_hints_for_new_type_ = false;
  }

  if (should_load_new_optimization_filter) {
    if (IsHintComponentProcessingDisabled()) {
      std::unique_ptr<proto::Configuration> manual_config =
          ParseComponentConfigFromCommandLine();
      if (manual_config->optimization_allowlists_size() > 0 ||
          manual_config->optimization_blocklists_size() > 0) {
        ProcessOptimizationFilters(manual_config->optimization_allowlists(),
                                   manual_config->optimization_blocklists());
      }
    } else {
      DCHECK(hints_component_info_);
      OnHintsComponentAvailable(*hints_component_info_);
    }
  } else {
    MaybeRunUpdateClosure(std::move(next_update_closure_));
  }
}

bool HintsManager::HasLoadedOptimizationAllowlist(
    proto::OptimizationType optimization_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return allowlist_optimization_filters_.find(optimization_type) !=
         allowlist_optimization_filters_.end();
}

bool HintsManager::HasLoadedOptimizationBlocklist(
    proto::OptimizationType optimization_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return blocklist_optimization_filters_.find(optimization_type) !=
         blocklist_optimization_filters_.end();
}

base::flat_map<proto::OptimizationType, OptimizationGuideDecisionWithMetadata>
HintsManager::GetDecisionsWithCachedInformationForURLAndOptimizationTypes(
    const GURL& url,
    const base::flat_set<proto::OptimizationType>& optimization_types) {
  base::flat_map<proto::OptimizationType, OptimizationGuideDecisionWithMetadata>
      decisions;

  for (const auto optimization_type : optimization_types) {
    OptimizationMetadata metadata;
    OptimizationTypeDecision type_decision =
        CanApplyOptimization(url, optimization_type, &metadata);
    OptimizationGuideDecision decision =
        GetOptimizationGuideDecisionFromOptimizationTypeDecision(type_decision);
    decisions[optimization_type] = {decision, metadata};
  }

  return decisions;
}

// TODO(crbug.com/40832354): Improve metrics coverage between all of these apis.
void HintsManager::CanApplyOptimization(
    const GURL& url,
    proto::OptimizationType optimization_type,
    OptimizationGuideDecisionCallback callback) {
  OptimizationMetadata metadata;
  OptimizationTypeDecision type_decision =
      CanApplyOptimization(url, optimization_type, &metadata);
  OptimizationGuideDecision decision =
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(type_decision);

  base::UmaHistogramEnumeration(
      base::StrCat({"OptimizationGuide.ApplyDecision.",
                    optimization_guide::GetStringNameForOptimizationType(
                        optimization_type)}),
      type_decision);

  std::move(callback).Run(decision, metadata);
}

void HintsManager::CanApplyOptimizationAsync(
    const GURL& navigation_url,
    proto::OptimizationType optimization_type,
    OptimizationGuideDecisionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OptimizationMetadata metadata;
  OptimizationTypeDecision type_decision =
      CanApplyOptimization(navigation_url, optimization_type, &metadata);
  OptimizationGuideDecision decision =
      GetOptimizationGuideDecisionFromOptimizationTypeDecision(type_decision);
  // It's possible that a hint that applies to |navigation_url| will come in
  // later, so only run the callback if we are sure we can apply the decision.
  if (decision == OptimizationGuideDecision::kTrue ||
      HasAllInformationForDecisionAvailable(navigation_url,
                                            optimization_type)) {
    base::UmaHistogramEnumeration(
        base::StrCat({"OptimizationGuide.ApplyDecision.",
                      GetStringNameForOptimizationType(optimization_type)}),
        type_decision);
    std::move(callback).Run(decision, metadata);
    return;
  }

  registered_callbacks_[navigation_url][optimization_type].push_back(
      std::move(callback));
}

OptimizationTypeDecision HintsManager::CanApplyOptimization(
    const GURL& navigation_url,
    proto::OptimizationType optimization_type,
    OptimizationMetadata* optimization_metadata) {
  return CanApplyOptimization(
      /*is_on_demand_request=*/false, navigation_url, optimization_type,
      hint_cache_->GetURLKeyedHint(navigation_url),
      hint_cache_->GetHostKeyedHintIfLoaded(navigation_url.GetHost()), false,
      optimization_metadata);
}

OptimizationTypeDecision HintsManager::CanApplyOptimization(
    bool is_on_demand_request,
    const GURL& url,
    proto::OptimizationType optimization_type,
    const proto::Hint* url_keyed_hint,
    const proto::Hint* host_keyed_hint,
    bool skip_cache,
    OptimizationMetadata* optimization_metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ScopedCanApplyOptimizationLogger scoped_logger(optimization_type, url,
                                                 optimization_guide_logger_);
  // Clear out optimization metadata if provided.
  if (optimization_metadata) {
    *optimization_metadata = {};
  }

  bool is_optimization_type_registered =
      registered_optimization_types_.find(optimization_type) !=
      registered_optimization_types_.end();

  // If type is not registered AND neither hint is passed in, we probably do not
  // have a hint, so just return.
  if (!is_optimization_type_registered &&
      (url_keyed_hint == nullptr && host_keyed_hint == nullptr)) {
    OptimizationTypeDecision type_decision =
        is_on_demand_request
            ? OptimizationTypeDecision::kNoHintAvailable
            : OptimizationTypeDecision::kRequestedUnregisteredType;
    scoped_logger.set_type_decision(type_decision);
    return type_decision;
  }

  // If the URL doesn't have a host, we cannot query the hint for it, so just
  // return early.
  if (!url.has_host()) {
    scoped_logger.set_type_decision(OptimizationTypeDecision::kInvalidURL);
    return OptimizationTypeDecision::kInvalidURL;
  }
  const auto& host = url.GetHost();

  // Check if the URL should be filtered out if we have an optimization filter
  // for the type.

  // Check if we have an allowlist loaded into memory for it, and if we do,
  // see if the URL matches anything in the filter.
  if (allowlist_optimization_filters_.find(optimization_type) !=
      allowlist_optimization_filters_.end()) {
    const auto type_decision =
        allowlist_optimization_filters_[optimization_type]->Matches(url)
            ? OptimizationTypeDecision::kAllowedByOptimizationFilter
            : OptimizationTypeDecision::kNotAllowedByOptimizationFilter;
    scoped_logger.set_type_decision(type_decision);
    return type_decision;
  }

  // Check if we have a blocklist loaded into memory for it, and if we do, see
  // if the URL matches anything in the filter.
  if (blocklist_optimization_filters_.find(optimization_type) !=
      blocklist_optimization_filters_.end()) {
    const auto type_decision =
        blocklist_optimization_filters_[optimization_type]->Matches(url)
            ? OptimizationTypeDecision::kNotAllowedByOptimizationFilter
            : OptimizationTypeDecision::kAllowedByOptimizationFilter;
    scoped_logger.set_type_decision(type_decision);
    return type_decision;
  }

  // Check if we had an optimization filter for it, but it was not loaded into
  // memory.
  if (optimization_types_with_filter_.find(optimization_type) !=
          optimization_types_with_filter_.end() ||
      pref_service_->GetDict(prefs::kPreviousOptimizationTypesWithFilter)
          .contains(optimization_guide::proto::OptimizationType_Name(
              optimization_type))) {
    scoped_logger.set_type_decision(
        OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime);
    return OptimizationTypeDecision::kHadOptimizationFilterButNotLoadedInTime;
  }

  // First, check if the optimization type is allowlisted by the URL-keyed hint.
  if (url_keyed_hint) {
    DCHECK_EQ(url_keyed_hint->page_hints_size(), 1);
    if (url_keyed_hint->page_hints_size() > 0) {
      if (IsOptimizationTypeAllowed(
              url_keyed_hint->page_hints(0).allowlisted_optimizations(),
              optimization_type, optimization_metadata)) {
        scoped_logger.set_type_decision(
            OptimizationTypeDecision::kAllowedByHint);
        if (optimization_metadata && !optimization_metadata->empty()) {
          scoped_logger.set_has_metadata();
        }
        return OptimizationTypeDecision::kAllowedByHint;
      }
    }
  }

  if (!host_keyed_hint) {
    // The request was not for cached information, so no data is available.
    if (skip_cache) {
      scoped_logger.set_type_decision(
          OptimizationTypeDecision::kNoHintAvailable);
      return OptimizationTypeDecision::kNoHintAvailable;
    }
    // Otherwise check and record cache status.
    if (hint_cache_->HasHint(host)) {
      // If we do not have a hint already loaded and we do not have one in the
      // cache, we do not know what to do with the URL so just return.
      // Otherwise, we do have information, but we just do not know it yet.
      if (features::ShouldPersistHintsToDisk()) {
        scoped_logger.set_type_decision(
            OptimizationTypeDecision::kHadHintButNotLoadedInTime);
        return OptimizationTypeDecision::kHadHintButNotLoadedInTime;
      } else {
        scoped_logger.set_type_decision(
            OptimizationTypeDecision::kNoHintAvailable);
        return OptimizationTypeDecision::kNoHintAvailable;
      }
    }

    scoped_logger.set_type_decision(OptimizationTypeDecision::kNoHintAvailable);
    return OptimizationTypeDecision::kNoHintAvailable;
  }

  if (IsOptimizationTypeAllowed(host_keyed_hint->allowlisted_optimizations(),
                                optimization_type, optimization_metadata)) {
    scoped_logger.set_type_decision(OptimizationTypeDecision::kAllowedByHint);
    if (optimization_metadata && !optimization_metadata->empty()) {
      scoped_logger.set_has_metadata();
    }
    return OptimizationTypeDecision::kAllowedByHint;
  }

  const proto::PageHint* matched_page_hint =
      host_keyed_hint ? FindPageHintForURL(url, host_keyed_hint) : nullptr;
  if (!matched_page_hint) {
    scoped_logger.set_type_decision(
        OptimizationTypeDecision::kNotAllowedByHint);
    return OptimizationTypeDecision::kNotAllowedByHint;
  }

  bool is_allowed =
      IsOptimizationTypeAllowed(matched_page_hint->allowlisted_optimizations(),
                                optimization_type, optimization_metadata);
  const auto type_decision = is_allowed
                                 ? OptimizationTypeDecision::kAllowedByHint
                                 : OptimizationTypeDecision::kNotAllowedByHint;
  scoped_logger.set_type_decision(type_decision);
  if (optimization_metadata && !optimization_metadata->empty()) {
    scoped_logger.set_has_metadata();
  }
  return type_decision;
}

void HintsManager::PrepareToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_callbacks_.find(navigation_url) ==
      registered_callbacks_.end()) {
    return;
  }

  LoadHintForHost(
      navigation_url.GetHost(),
      base::BindOnce(&HintsManager::OnReadyToInvokeRegisteredCallbacks,
                     weak_ptr_factory_.GetWeakPtr(), navigation_url));
}

void HintsManager::OnReadyToInvokeRegisteredCallbacks(
    const GURL& navigation_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_callbacks_.find(navigation_url) ==
      registered_callbacks_.end()) {
    return;
  }

  for (auto& opt_type_and_callbacks :
       registered_callbacks_.at(navigation_url)) {
    proto::OptimizationType opt_type = opt_type_and_callbacks.first;

    for (auto& callback : opt_type_and_callbacks.second) {
      OptimizationMetadata metadata;
      OptimizationTypeDecision type_decision =
          CanApplyOptimization(navigation_url, opt_type, &metadata);
      OptimizationGuideDecision decision =
          GetOptimizationGuideDecisionFromOptimizationTypeDecision(
              type_decision);
      base::UmaHistogramEnumeration(
          base::StrCat({"OptimizationGuide.ApplyDecision.",
                        GetStringNameForOptimizationType(opt_type)}),
          type_decision);
      std::move(callback).Run(decision, metadata);
    }
  }
  registered_callbacks_.erase(navigation_url);
}

void HintsManager::OnNavigationStartOrRedirect(
    OptimizationGuideNavigationData* navigation_data,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (registered_optimization_types_.empty()) {
    // Do not attempt to load anything since we have nothing we need to get data
    // for.
    std::move(callback).Run();
    return;
  }

  LoadHintForURL(navigation_data->navigation_url(), std::move(callback));

}

void HintsManager::OnNavigationFinish(
    const std::vector<GURL>& navigation_redirect_chain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The callbacks will be invoked when the fetch request comes back, so it
  // will be cleaned up later.
  for (const auto& url : navigation_redirect_chain) {
    PrepareToInvokeRegisteredCallbacks(url);
  }
}

base::WeakPtr<OptimizationGuideStore> HintsManager::hint_store() {
  return hint_cache_->hint_store();
}

HintCache* HintsManager::hint_cache() {
  return hint_cache_.get();
}

PushNotificationManager* HintsManager::push_notification_manager() {
  return push_notification_manager_.get();
}

bool HintsManager::HasAllInformationForDecisionAvailable(
    const GURL& navigation_url,
    proto::OptimizationType optimization_type) {
  if (HasLoadedOptimizationAllowlist(optimization_type) ||
      HasLoadedOptimizationBlocklist(optimization_type)) {
    // If we have an optimization filter for the optimization type, it is
    // consulted instead of any hints that may be available.
    return true;
  }

  bool has_host_keyed_hint = hint_cache_->HasHint(navigation_url.GetHost());
  const auto* host_keyed_hint =
      hint_cache_->GetHostKeyedHintIfLoaded(navigation_url.GetHost());
  if (has_host_keyed_hint && host_keyed_hint == nullptr) {
    // If we have a host-keyed hint in the cache and it is not loaded, we do not
    // have all information available, regardless of whether we can fetch hints
    // or not.
    return false;
  }

  // If we are allowed to fetch hints for the navigation, we only have all
  // information available for certain if we have attempted to get the URL-keyed
  // hint and if the host-keyed hint is loaded.
  return hint_cache_->HasURLKeyedEntryForURL(navigation_url) &&
         host_keyed_hint != nullptr;
}

void HintsManager::ClearFetchedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hint_cache_->ClearFetchedHints();
}

void HintsManager::ClearHostKeyedHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  hint_cache_->ClearHostKeyedHints();
}

void HintsManager::AddHintForTesting(
    const GURL& url,
    proto::OptimizationType optimization_type,
    const std::optional<OptimizationMetadata>& metadata) {
  AddHintWithMultipleOptimizationsForTesting(
      url, {{optimization_type, metadata}});
}

void HintsManager::AddHintWithMultipleOptimizationsForTesting(
    const GURL& url,
    const std::vector<optimization_guide::proto::OptimizationType>&
        optimization_types) {
  std::unique_ptr<proto::Hint> hint = std::make_unique<proto::Hint>();
  hint->set_key(url.spec());
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  for (proto::OptimizationType optimization_type : optimization_types) {
    proto::Optimization* optimization =
        page_hint->add_allowlisted_optimizations();
    optimization->set_optimization_type(optimization_type);
  }
  hint_cache_->AddHintForTesting(url, std::move(hint));  // IN-TEST
  PrepareToInvokeRegisteredCallbacks(url);
}

void HintsManager::AddHintWithMultipleOptimizationsForTesting(
    const GURL& url,
    const std::vector<
        std::pair<optimization_guide::proto::OptimizationType,
                  std::optional<optimization_guide::OptimizationMetadata>>>&
        optimization_types_and_metadata) {
  std::unique_ptr<proto::Hint> hint = std::make_unique<proto::Hint>();
  hint->set_key(url.spec());
  proto::PageHint* page_hint = hint->add_page_hints();
  page_hint->set_page_pattern("*");
  for (const auto& [optimization_type, metadata] :
       optimization_types_and_metadata) {
    proto::Optimization* optimization =
        page_hint->add_allowlisted_optimizations();
    optimization->set_optimization_type(optimization_type);
    if (!metadata) {
      continue;
    }
    if (metadata->loading_predictor_metadata()) {
      *optimization->mutable_loading_predictor_metadata() =
          *metadata->loading_predictor_metadata();
    } else if (metadata->any_metadata()) {
      *optimization->mutable_any_metadata() = *metadata->any_metadata();
    } else {
      NOTREACHED();
    }
  }
  hint_cache_->AddHintForTesting(url, std::move(hint));  // IN-TEST
  PrepareToInvokeRegisteredCallbacks(url);
}

void HintsManager::RemoveFetchedEntriesByHintKeys(
    base::OnceClosure on_success,
    proto::KeyRepresentation key_representation,
    const base::flat_set<std::string>& hint_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Make sure the key representation is something that we expect.
  switch (key_representation) {
    case proto::KeyRepresentation::HOST:
    case proto::KeyRepresentation::FULL_URL:
      break;
    default:
      NOTREACHED();
  }

  if (key_representation == proto::FULL_URL) {
    base::flat_set<GURL> urls_to_remove;
    base::flat_set<std::string> hosts_to_remove;
    // It is possible that the hints may have upgraded from being HOST keyed to
    // URL keyed on the server at any time. To protect against this, also remove
    // the host of the GURL from storage.
    // However, note that the opposite is not likely to happen since URL-keyed
    // hints are not persisted to disk.
    for (const std::string& url : hint_keys) {
      GURL gurl(url);
      if (!gurl.is_valid()) {
        continue;
      }
      hosts_to_remove.insert(gurl.GetHost());
      urls_to_remove.insert(gurl);
    }

    // Also clear the HintFetcher's host pref.

    hint_cache_->RemoveHintsForURLs(urls_to_remove);
    hint_cache_->RemoveHintsForHosts(std::move(on_success), hosts_to_remove);
    return;
  }

  // Also clear the HintFetcher's host pref.

  hint_cache_->RemoveHintsForHosts(std::move(on_success), hint_keys);
}

}  // namespace optimization_guide
