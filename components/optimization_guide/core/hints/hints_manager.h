// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_HINTS_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_HINTS_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "components/optimization_guide/core/filters/hints_component_info.h"
#include "components/optimization_guide/core/filters/optimization_hints_component_observer.h"
#include "components/optimization_guide/core/hints/insertion_ordered_set.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/push_notification_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/hints.pb.h"

class OptimizationGuideLogger;
class OptimizationGuideNavigationData;
class OptimizationGuideTestAppInterfaceWrapper;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace optimization_guide {
class HintCache;
class OptimizationFilter;
class OptimizationGuideStore;
class OptimizationMetadata;
enum class OptimizationTypeDecision;
class StoreUpdateData;
class TabUrlProvider;
class TopHostProvider;

// Whether batch updates are enabled for active tabs and top hosts.
// TODO: crbug.com/421924837 - This only exists to allow tests to exercise this
// behavior on platforms where it is disabled. Fix tests and remove this.
BASE_DECLARE_FEATURE(kHintsBatchUpdateForActiveTabsAndTopHosts);

// The local histogram used to record that the component hints are stored in
// the cache and are ready for use.
extern const char kLoadedHintLocalHistogramString[];

class HintsManager : public OptimizationHintsComponentObserver,
                     public PushNotificationManager::Delegate {
 public:
  HintsManager(
      bool is_off_the_record,
      const std::string& application_locale,
      PrefService* pref_service,
      base::WeakPtr<OptimizationGuideStore> hint_store,
      TopHostProvider* top_host_provider,
      TabUrlProvider* tab_url_provider,
      std::unique_ptr<PushNotificationManager> push_notification_manager,
      signin::IdentityManager* identity_manager,
      OptimizationGuideLogger* optimization_guide_logger);

  ~HintsManager() override;

  HintsManager(const HintsManager&) = delete;
  HintsManager& operator=(const HintsManager&) = delete;

  // Unhooks the observer to |optimization_guide_service_|.
  void Shutdown();

  // Returns the OptimizationGuideDecision from |optimization_type_decision|.
  static OptimizationGuideDecision
  GetOptimizationGuideDecisionFromOptimizationTypeDecision(
      OptimizationTypeDecision optimization_type_decision);

  // OptimizationHintsComponentObserver implementation:
  void OnHintsComponentAvailable(const HintsComponentInfo& info) override;

  // |next_update_closure| is called the next time OnHintsComponentAvailable()
  // is called and the corresponding hints have been updated.
  void ListenForNextUpdateForTesting(base::OnceClosure next_update_closure);

  // Registers the optimization types that have the potential for hints to be
  // called by consumers of the Optimization Guide.
  void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types);

  // Returns the optimization types that are registered.
  base::flat_set<proto::OptimizationType> registered_optimization_types()
      const {
    return registered_optimization_types_;
  }

  // Returns whether there is an optimization allowlist loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationAllowlist(
      proto::OptimizationType optimization_type);
  // Returns whether there is an optimization blocklist loaded for
  // |optimization_type|.
  bool HasLoadedOptimizationBlocklist(
      proto::OptimizationType optimization_type);

  // Returns the OptimizationTypeDecision based on the given parameters.
  // |optimization_metadata| will be populated, if applicable.
  OptimizationTypeDecision CanApplyOptimization(
      const GURL& navigation_url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata);

  // Returns the OptimizationTypeDecision based on the given parameters.
  // |optimization_metadata| will be populated, if applicable. The decision will
  // be computed on |url_keyed_hint| or |host_keyed_hint| if possible.
  // |skip_cache| will be used to determine if the decision is unknown.
  OptimizationTypeDecision CanApplyOptimization(
      bool is_on_demand_request,
      const GURL& url,
      proto::OptimizationType optimization_type,
      const proto::Hint* url_keyed_hint,
      const proto::Hint* host_keyed_hint,
      bool skip_cache,
      OptimizationMetadata* optimization_metadata);

  // Invokes |callback| with the decision for the URL contained in |url| and
  // |optimization_type|, when sufficient information has been collected to
  // make the decision.
  virtual void CanApplyOptimization(
      const GURL& url,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback);

  // Invokes |callback| with the decision for |navigation_url| and
  // |optimization_type|, when sufficient information has been collected by
  // |this| to make the decision. Virtual for testing.
  virtual void CanApplyOptimizationAsync(
      const GURL& navigation_url,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback);

  // Clears all fetched hints from |hint_cache_|.
  void ClearFetchedHints();

  // Clears the host-keyed fetched hints from |hint_cache_|, both the persisted
  // and in memory ones.
  void ClearHostKeyedHints();

  // Overrides |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Notifies |this| that a navigation with |navigation_data| started.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void OnNavigationStartOrRedirect(
      OptimizationGuideNavigationData* navigation_data,
      base::OnceClosure callback);

  // Notifies |this| that a navigation with redirect chain
  // |navigation_redirect_chain| has finished.
  void OnNavigationFinish(const std::vector<GURL>& navigation_redirect_chain);

  // PushNotificationManager::Delegate:
  void RemoveFetchedEntriesByHintKeys(
      base::OnceClosure on_success,
      proto::KeyRepresentation key_representation,
      const base::flat_set<std::string>& hint_keys) override;

  // Returns the hint cache for |this|.
  HintCache* hint_cache();

  // Returns the persistent store for |this|.
  base::WeakPtr<OptimizationGuideStore> hint_store();

  // Returns the push notification manager for |this|. May be nullptr;
  PushNotificationManager* push_notification_manager();

  // Add hints to the cache with the provided metadata. For testing only.
  void AddHintForTesting(const GURL& url,
                         proto::OptimizationType optimization_type,
                         const std::optional<OptimizationMetadata>& metadata);

  // Add hints to the cache for the provided optimization types. For testing
  // only.
  void AddHintWithMultipleOptimizationsForTesting(
      const GURL& url,
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types);

  // Add hints to the cache for the provided optimization types and metadata.
  // For testing only.
  void AddHintWithMultipleOptimizationsForTesting(
      const GURL& url,
      const std::vector<
          std::pair<optimization_guide::proto::OptimizationType,
                    std::optional<optimization_guide::OptimizationMetadata>>>&
          optimization_types_and_metadata);

 private:
  friend class ::OptimizationGuideTestAppInterfaceWrapper;
  friend class HintsManagerTest;

  // Processes the optimization filters contained in the hints component.
  void ProcessOptimizationFilters(
      const google::protobuf::RepeatedPtrField<proto::OptimizationFilter>&
          allowlist_optimization_filters,
      const google::protobuf::RepeatedPtrField<proto::OptimizationFilter>&
          blocklist_optimization_filters);

  // Process a set of optimization filters.
  //
  // |is_allowlist| will be used to ensure that the filters are either uses as
  // allowlists or blocklists.
  void ProcessOptimizationFilterSet(const google::protobuf::RepeatedPtrField<
                                        proto::OptimizationFilter>& filters,
                                    bool is_allowlist);

  // Callback run after the hint cache is fully initialized. At this point,
  // the HintsManager is ready to process hints.
  void OnHintCacheInitialized();

  // Updates the cache with the latest hints sent by the Component Updater.
  void UpdateComponentHints(base::OnceClosure update_closure,
                            std::unique_ptr<StoreUpdateData> update_data,
                            std::unique_ptr<proto::Configuration> config);

  // Called when the hints have been fully updated with the latest hints from
  // the Component Updater. This is used as a signal during tests.
  void OnComponentHintsUpdated(base::OnceClosure update_closure,
                               bool hints_updated);

  // Returns the URLs that are currently in the active tab model that do not
  // have a hint available in |hint_cache_|.
  const std::vector<GURL> GetActiveTabURLsToRefresh();

  bool HasPersonalizableTypesRegistered();

  // Returns decisions for |url| and |optimization_types| based on what's cached
  // locally.
  base::flat_map<proto::OptimizationType, OptimizationGuideDecisionWithMetadata>
  GetDecisionsWithCachedInformationForURLAndOptimizationTypes(
      const GURL& url,
      const base::flat_set<proto::OptimizationType>& optimization_types);

  // Called when the request to load a hint has completed.
  void OnHintLoaded(base::OnceClosure callback,
                    const proto::Hint* loaded_hint) const;

  // Loads the hint if available for navigation to |url|.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that load or not. The callback can be used as a
  // signal for tests.
  void LoadHintForURL(const GURL& url, base::OnceClosure callback);

  // Loads the hint for |host| if available.
  // |callback| is run when the request has finished regardless of whether there
  // was actually a hint for that |host| or not. The callback can be used as a
  // signal for tests.
  void LoadHintForHost(const std::string& host, base::OnceClosure callback);

  // If an entry for |navigation_url| is contained in |registered_callbacks_|,
  // it will load the hint for |navigation_url|'s host and upon completion, will
  // invoke the registered callbacks for |navigation_url|.
  void PrepareToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Invokes the registered callbacks for |navigation_url|, if applicable.
  void OnReadyToInvokeRegisteredCallbacks(const GURL& navigation_url);

  // Whether all information was available to make a decision for
  // |navigation_url| and |optimization type}.
  bool HasAllInformationForDecisionAvailable(
      const GURL& navigation_url,
      proto::OptimizationType optimization_type);

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // The information of the latest component delivered by
  // |optimization_guide_service_|.
  std::optional<HintsComponentInfo> hints_component_info_;

  // The component version that failed to process in the last session, if
  // applicable.
  const std::optional<base::Version> failed_component_version_;

  // The version of the component that is currently being processed.
  std::optional<base::Version> currently_processing_component_version_;

  // The set of optimization types that have been registered with the hints
  // manager.
  //
  // Should only be read and modified on the UI thread.
  base::flat_set<proto::OptimizationType> registered_optimization_types_;

  // The set of optimization types that the component specified by
  // |component_info_| has optimization filters for.
  base::flat_set<proto::OptimizationType> optimization_types_with_filter_;

  // A map from optimization type to the host filter that holds the allowlist
  // for that type.
  base::flat_map<proto::OptimizationType, std::unique_ptr<OptimizationFilter>>
      allowlist_optimization_filters_;

  // A map from optimization type to the host filter that holds the blocklist
  // for that type.
  base::flat_map<proto::OptimizationType, std::unique_ptr<OptimizationFilter>>
      blocklist_optimization_filters_;

  // A map from URL to a map of callbacks keyed by their optimization type.
  base::flat_map<GURL,
                 base::flat_map<proto::OptimizationType,
                                std::vector<OptimizationGuideDecisionCallback>>>
      registered_callbacks_;

  // Whether |this| was created for an off the record profile.
  const bool is_off_the_record_;

  // The current applcation locale of Chrome.
  const std::string application_locale_;

  // A reference to the PrefService for this profile. Not owned.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The hint cache that holds both hints received from the component and
  // fetched from the remote Optimization Guide Service.
  std::unique_ptr<HintCache> hint_cache_;


  // The top host provider that can be queried. Not owned.
  raw_ptr<TopHostProvider> top_host_provider_ = nullptr;

  // The tab URL provider that can be queried. Not owned.
  raw_ptr<TabUrlProvider> tab_url_provider_ = nullptr;

  // The timer used to schedule fetching hints from the remote Optimization
  // Guide Service.
  base::OneShotTimer active_tabs_hints_fetch_timer_;

  // The class that handles push notification processing and informs |this| of
  // what to do through the implemented Delegate above.
  std::unique_ptr<PushNotificationManager> push_notification_manager_;

  // Unowned IdentityManager for fetching access tokens. Could be null for
  // incognito profiles.
  const raw_ptr<signin::IdentityManager> identity_manager_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  raw_ptr<const base::Clock> clock_;

  // Whether fetched hints should be cleared when the store is initialized
  // because a new optimization type was registered.
  bool should_clear_hints_for_new_type_ = false;

  // Used in testing to subscribe to an update event in this class.
  base::OnceClosure next_update_closure_;

  // Background thread where hints processing should be performed.
  //
  // Warning: This must be the last object, so it is destroyed (and flushed)
  // first. This will prevent use-after-free issues where the background thread
  // would access other member variables after they have been destroyed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Optimization types for which proactive personalization is enabled.
  const features::OptimizationTypeSet
      allowed_optimization_types_for_proactive_personalization_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self.
  base::WeakPtrFactory<HintsManager> weak_ptr_factory_{this};

};

// Overrides the Hints Protobuf that would come from the component updater. If
// the value of this switch is invalid, regular hint processing is used.
// The value of this switch should be a base64 encoding of a binary
// Configuration message, found in optimization_guide's hints.proto. Providing a
// valid value to this switch causes Chrome startup to block on hints parsing.
inline constexpr char kHintsProtoOverrideSwitch[] =
    "optimization_guide_hints_override";

// Overrides the hints fetch scheduling and delay, causing a hints fetch
// immediately on start up using the TopHostProvider. This is meant for testing.
inline constexpr char kFetchHintsOverrideTimerSwitch[] =
    "optimization-guide-fetch-hints-override-timer";

// Purges the store of all hints before loading any new hints on start up.
inline constexpr char kPurgeHintsStoreSwitch[] =
    "purge-optimization-guide-store";

// Disables fetching of hints in real-time at the time of navigation start.
// Meant for testing only.
inline constexpr char
    kDisableFetchingHintsAtNavigationStartForTestingSwitch[] =
        "disable-fetching-hints-at-navigation-start";

// Attempts to parse a base64 encoded Optimization Guide Configuration proto
// from the command line. If no proto is given or if it is encoded incorrectly,
// nullptr is returned.
std::unique_ptr<proto::Configuration>
ParseComponentConfigFromCommandLine();

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_HINTS_MANAGER_H_
