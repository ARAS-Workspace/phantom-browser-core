// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/types/optional_ref.h"
#include "components/download/public/background_service/download_params.h"
#include "components/optimization_guide/core/delivery/model_enums.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace unzip::mojom {
class Unzipper;
}  // namespace unzip::mojom

namespace unzip {
// TODO: crbug.com/421262905 - Avoid duplicating this alias.
using UnzipperFactory =
    base::RepeatingCallback<mojo::PendingRemote<mojom::Unzipper>()>;
}  // namespace unzip

class OptimizationGuideLogger;
class PrefService;

namespace optimization_guide {

class OptimizationTargetModelObserver;
class PredictionModelStore;
class ProfileDownloadServiceTracker;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager : public OptimizationGuideModelProvider {
 public:
  PredictionManager(
      PredictionModelStore* prediction_model_store,
      PrefService* local_state,
      const std::string& application_locale,
      OptimizationGuideLogger* optimization_guide_logger,
      unzip::UnzipperFactory unzipper_factory);

  PredictionManager(const PredictionManager&) = delete;
  PredictionManager& operator=(const PredictionManager&) = delete;

  ~PredictionManager() override;

  // Return the optimization targets that are registered.
  base::flat_set<proto::OptimizationTarget> GetRegisteredOptimizationTargets()
      const;

  // Override the model file returned to observers for |optimization_target|.
  // Use ModelInfo aggregate initialization to construct the model files. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      proto::OptimizationTarget optimization_target,
      std::optional<ModelInfo> model_info);

  // PredictionModelDownloadObserver:
  void OnModelReady(const base::FilePath& base_model_dir,
                    const proto::PredictionModel& model);
  std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
  GetDownloadedModelsInfoForWebUI() const;

  base::flat_map<std::string, bool> GetOnDeviceSupplementaryModelsInfoForWebUI()
      const;

  // OptimizationGuideModelProvider:
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const std::optional<proto::Any>& model_metadata,
      scoped_refptr<base::SequencedTaskRunner> model_task_runner,
      OptimizationTargetModelObserver* observer) override;
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer) override;
  void SetModelDownloadSchedulingParams(
      proto::OptimizationTarget optimization_target,
      const download::SchedulingParams& params) override;

 protected:
  // Process `prediction_models` to be stored in the in memory optimization
  // target prediction model map for immediate use and asynchronously write the
  // models to the model and features store to be persisted.
  // `models_request_info` is the list of models the fetch request was made
  // for, and `prediction_models` is the models received in response. Any models
  // missing in the response will be deleted from the store, since the remote
  // optimization guide service has no models for them.
  void UpdatePredictionModels(
      const std::vector<proto::ModelInfo>& models_request_info,
      const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
          prediction_models);

 private:
  friend class PredictionManagerTestBase;
  friend class PredictionModelStoreBrowserTestBase;

  // Gets the model task runner to use for the target.
  scoped_refptr<base::SequencedTaskRunner> GetModelTaskRunner(
      proto::OptimizationTarget optimization_target);

  // Load models for every target in |optimization_targets| that have not yet
  // been loaded from the store.
  void LoadPredictionModels(
      const base::flat_set<proto::OptimizationTarget>& optimization_targets);

  // Callback run after prediction models are stored in
  // `prediction_model_store_`.
  void OnPredictionModelsStored();

  // Callback run after a prediction model is loaded from the store.
  // |prediction_model| is used to construct a PredictionModel capable of making
  // prediction for the appropriate |optimization_target|.
  void OnLoadPredictionModel(proto::OptimizationTarget optimization_target,
                             bool record_availability_metrics,
                             std::optional<ModelInfo> model_info);

  // Callback run after a prediction model is loaded from a command-line
  // override.
  void OnPredictionModelOverrideLoaded(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Removes the model for `optimization_target` from store, for the
  // `model_removal_reason`.
  void RemoveModelFromStore(
      proto::OptimizationTarget optimization_target,
      PredictionModelStoreModelRemovalReason model_removal_reason);

  // Return whether the model stored in memory for |optimization_target| should
  // be updated based on what's currently stored and |new_version|.
  bool ShouldUpdateStoredModelForTarget(
      proto::OptimizationTarget optimization_target,
      int64_t new_version) const;

  // Updates the in-memory model file for `optimization_target` to
  // `model_info`.
  void StoreLoadedModelInfo(proto::OptimizationTarget optimization_target,
                            ModelInfo model_info);

  // Returns a new file path for the directory to download the model files for
  // |optimization_target|. The directory will not be created.
  base::FilePath GetBaseModelDirForDownload(
      proto::OptimizationTarget optimization_target);

  void SetModelCacheKeyForTesting(const ClientCacheKey& model_cache_key) {
    model_cache_key_ = model_cache_key;
  }

  ModelProviderRegistry registry_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The new optimization guide model store. Will be null when the feature is
  // not enabled. Not owned and outlives |this| since its an install-wide store.
  raw_ptr<PredictionModelStore> prediction_model_store_;

  // The logger that plumbs the debug logs to the optimization guide
  // internals page. Not owned. Guaranteed to outlive |this|, since the logger
  // and |this| are owned by the optimization guide keyed service.
  raw_ptr<OptimizationGuideLogger> optimization_guide_logger_;

  // Custom scheduling params that can be set for a given optimization target.
  // If an entry is present for a given target, it will be used instead of the
  // default params.
  base::flat_map<proto::OptimizationTarget, download::SchedulingParams>
      custom_scheduling_params_;

  // Callback to build Unzipper remotes.
  unzip::UnzipperFactory unzipper_factory_;

  // The task runner to use if AddObserverForOptimizationTargetModel was never
  // invoked to provide one.
  const scoped_refptr<base::SequencedTaskRunner> default_model_task_runner_;

  // The task runner on which to run model loading.
  base::flat_map<proto::OptimizationTarget,
                 scoped_refptr<base::SequencedTaskRunner>>
      optimization_target_model_task_runner_;

  // Time the prediction manager got initialized.
  // TODO(crbug.com/40861855): Remove this old model store once the new model
  // store is launched.
  base::TimeTicks init_time_;

  // The locale of the application.
  std::string application_locale_;

  // Model cache key for the profile.
  ClientCacheKey model_cache_key_;

  // The path to the directory containing the models.
  base::FilePath models_dir_path_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_PREDICTION_MANAGER_H_
