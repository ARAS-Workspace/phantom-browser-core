// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/prediction_manager.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/observer_list.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/model_store_metadata_entry.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/delivery/optimization_target_model_observer.h"
#include "components/optimization_guide/core/delivery/prediction_model_override.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals.mojom.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/services/unzip/public/cpp/unzip.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace optimization_guide {

namespace {

void RecordModelUpdateVersion(const proto::ModelInfo& model_info) {
  base::UmaHistogramSparse(
      base::StrCat({"OptimizationGuide.PredictionModelUpdateVersion.",
                    GetStringNameForOptimizationTarget(
                        model_info.optimization_target())}),
      model_info.version());
}

void RecordModelAvailableAtRegistration(
    proto::OptimizationTarget optimization_target,
    bool model_available_at_registration) {
  base::UmaHistogramBoolean(
      base::StrCat(
          {"OptimizationGuide.PredictionManager.ModelAvailableAtRegistration.",
           GetStringNameForOptimizationTarget(optimization_target)}),
      model_available_at_registration);
}

}  // namespace

PredictionManager::PredictionManager(
    PredictionModelStore* prediction_model_store,
    PrefService* local_state,
    const std::string& application_locale,
    OptimizationGuideLogger* optimization_guide_logger,
    unzip::UnzipperFactory unzipper_factory)
    : registry_(optimization_guide_logger),
      prediction_model_store_(prediction_model_store),
      optimization_guide_logger_(optimization_guide_logger),
      unzipper_factory_(std::move(unzipper_factory)),
      default_model_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
      application_locale_(application_locale),
      model_cache_key_(ClientCacheKey::FromLocale(application_locale_)) {
  DCHECK(prediction_model_store_);
  LoadPredictionModels(GetRegisteredOptimizationTargets());
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.StoreInitialized", true);
}

PredictionManager::~PredictionManager() {
}

void PredictionManager::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const std::optional<proto::Any>& model_metadata,
    scoped_refptr<base::SequencedTaskRunner> model_task_runner,
    OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Set the target's task runner if none is already present. This has the
  // effect of using the first registered model_task_runner for all model
  // execution, if more than one observer gets registered (e.g. in the case of
  // multiple profiles using the model).
  optimization_target_model_task_runner_.emplace(optimization_target,
                                                 model_task_runner);

  registry_.AddObserverForOptimizationTargetModel(
      optimization_target, model_metadata, model_task_runner, observer);

  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Registered new OptimizationTarget: " << optimization_target;
  }

  // Otherwise, load prediction models for any newly registered targets.
  LoadPredictionModels({optimization_target});
}

void PredictionManager::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registry_.RemoveObserverForOptimizationTargetModel(optimization_target,
                                                     observer);
}

void PredictionManager::SetModelDownloadSchedulingParams(
    proto::OptimizationTarget optimization_target,
    const download::SchedulingParams& params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  custom_scheduling_params_.insert_or_assign(optimization_target, params);
}

base::flat_set<proto::OptimizationTarget>
PredictionManager::GetRegisteredOptimizationTargets() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registry_.GetRegisteredOptimizationTargets();
}

scoped_refptr<base::SequencedTaskRunner> PredictionManager::GetModelTaskRunner(
    proto::OptimizationTarget optimization_target) {
  const auto loc =
      optimization_target_model_task_runner_.find(optimization_target);
  return loc != optimization_target_model_task_runner_.end()
             ? loc->second
             : default_model_task_runner_;
}

void PredictionManager::OnModelReady(const base::FilePath& base_model_dir,
                                     const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model.model_info().has_version() &&
         model.model_info().has_optimization_target());

  TRACE_EVENT("optimization_guide", "PredictionManager::OnModelReady", "target",
              GetStringNameForOptimizationTarget(
                  model.model_info().optimization_target()));

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  if (overrides.Get(model.model_info().optimization_target())) {
    // Skip updating the model if override is present.
    return;
  }

  RecordModelUpdateVersion(model.model_info());
  ModelProviderRegistry::RecordLifecycleState(
      model.model_info().optimization_target(),
      ModelDeliveryEvent::kModelDownloaded);
  if (optimization_guide_logger_->ShouldEnableDebugLogs()) {
    OPTIMIZATION_GUIDE_LOGGER(
        optimization_guide_common::mojom::LogSource::MODEL_MANAGEMENT,
        optimization_guide_logger_)
        << "Model Files Downloaded target: "
        << model.model_info().optimization_target() << "\nNew Version: "
        << base::NumberToString(model.model_info().version());
  }

  // Store the received model in the store.
  prediction_model_store_->UpdateModel(
      model.model_info().optimization_target(), model_cache_key_,
      model.model_info(), base_model_dir,
      base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));

  if (registry_.IsRegistered(model.model_info().optimization_target())) {
    OnLoadPredictionModel(model.model_info().optimization_target(),
                          /*record_availability_metrics=*/false,
                          ModelInfo::CreateFromProto(model));
  }
}

std::vector<optimization_guide_internals::mojom::DownloadedModelInfoPtr>
PredictionManager::GetDownloadedModelsInfoForWebUI() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return registry_.GetDownloadedModelsInfoForWebUI();
}

base::flat_map<std::string, bool>
PredictionManager::GetOnDeviceSupplementaryModelsInfoForWebUI() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<proto::OptimizationTarget> supp_targets = {
      proto::OptimizationTarget::OPTIMIZATION_TARGET_GENERALIZED_SAFETY,
      proto::OptimizationTarget::OPTIMIZATION_TARGET_LANGUAGE_DETECTION};
  base::flat_map<std::string, bool> supp_models_info;
  for (const auto target : supp_targets) {
    supp_models_info[proto::OptimizationTarget_Name(target)] =
        !!registry_.GetModel(target);
  }

  return supp_models_info;
}

void PredictionManager::OnPredictionModelsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::OnPredictionModelOverrideLoaded(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<proto::PredictionModel> prediction_model) {
  const bool is_available = prediction_model != nullptr;
  VLOG(0) << "Loading override for "
          << proto::OptimizationTarget_Name(optimization_target)
          << (is_available ? " succeeded" : " failed");
  std::optional<ModelInfo> model_info;
  if (prediction_model) {
    model_info = ModelInfo::CreateFromProto(*prediction_model);
  }
  OnLoadPredictionModel(optimization_target,
                        /*record_availability_metrics=*/false,
                        std::move(model_info));
  RecordModelAvailableAtRegistration(optimization_target, is_available);
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto overrides = PredictionModelOverrides::ParseFromCommandLine(
      base::CommandLine::ForCurrentProcess());
  for (proto::OptimizationTarget optimization_target : optimization_targets) {
    // Give preference to any overrides given on the command line.
    if (auto* entry = overrides.Get(optimization_target); entry) {
      base::FilePath base_model_dir =
          GetBaseModelDirForDownload(optimization_target);
      entry->BuildModel(
          base_model_dir, unzipper_factory_,
          base::BindOnce(&PredictionManager::OnPredictionModelOverrideLoaded,
                         ui_weak_ptr_factory_.GetWeakPtr(),
                         optimization_target));
      continue;
    }

    if (!prediction_model_store_->HasModel(optimization_target,
                                           model_cache_key_)) {
      RecordModelAvailableAtRegistration(optimization_target, false);
      continue;
    }
    prediction_model_store_->LoadModel(
        optimization_target, model_cache_key_,
        GetModelTaskRunner(optimization_target),
        base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr(), optimization_target,
                       /*record_availability_metrics=*/true));
  }
}

void PredictionManager::OnLoadPredictionModel(
    proto::OptimizationTarget optimization_target,
    bool record_availability_metrics,
    std::optional<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UmaHistogramBoolean(
      base::StrCat({"OptimizationGuide.IsPredictionModelValid.",
                    GetStringNameForOptimizationTarget(optimization_target)}),
      model_info.has_value());

  bool success = model_info && registry_.IsRegistered(optimization_target);
  if (record_availability_metrics) {
    RecordModelAvailableAtRegistration(optimization_target, success);
  }
  if (success) {
    int64_t version = model_info->version;
    if (ShouldUpdateStoredModelForTarget(optimization_target, version)) {
      StoreLoadedModelInfo(optimization_target, std::move(*model_info));
    }
    base::UmaHistogramSparse(
        base::StrCat({"OptimizationGuide.PredictionModelLoadedVersion.",
                      GetStringNameForOptimizationTarget(optimization_target)}),
        version);
  } else {
    RemoveModelFromStore(
        optimization_target,
        PredictionModelStoreModelRemovalReason::kModelLoadFailed);
  }
}

void PredictionManager::RemoveModelFromStore(
    proto::OptimizationTarget optimization_target,
    PredictionModelStoreModelRemovalReason model_removal_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (prediction_model_store_->HasModel(optimization_target,
                                        model_cache_key_)) {
    prediction_model_store_->RemoveModel(optimization_target, model_cache_key_,
                                         model_removal_reason);
    registry_.RemoveModel(optimization_target);
  }
}

bool PredictionManager::ShouldUpdateStoredModelForTarget(
    proto::OptimizationTarget optimization_target,
    int64_t new_version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (const ModelInfo* info = registry_.GetModel(optimization_target); info) {
    return info->version != new_version;
  }
  return true;
}

void PredictionManager::StoreLoadedModelInfo(
    proto::OptimizationTarget optimization_target,
    ModelInfo model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  registry_.UpdateModel(optimization_target, std::move(model_info));
}

base::FilePath PredictionManager::GetBaseModelDirForDownload(
    proto::OptimizationTarget optimization_target) {
  return prediction_model_store_->GetBaseModelDirForModelCacheKey(
      optimization_target, model_cache_key_);
}

void PredictionManager::OverrideTargetModelForTesting(
    proto::OptimizationTarget optimization_target,
    std::optional<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (model_info) {
    registry_.UpdateModelImmediatelyForTesting(  // IN-TEST
        optimization_target, std::move(*model_info));
  } else {
    registry_.RemoveModel(optimization_target);
  }
}

}  // namespace optimization_guide
