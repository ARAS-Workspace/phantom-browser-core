// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_utils.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/history_embeddings/core/history_embeddings_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/model_execution_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"

namespace history_embeddings {

constexpr auto kEnabledByDefaultForDesktopOnly =
#if BUILDFLAG(IS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT;
#else
    base::FEATURE_ENABLED_BY_DEFAULT;
#endif

// These are the kill switches for the launched history embeddings features.
BASE_FEATURE(kLaunchedHistoryEmbeddings, kEnabledByDefaultForDesktopOnly);

bool IsHistoryEmbeddingsEnabledForProfile(Profile* profile) {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service
             ->ShouldFeatureBeCurrentlyEnabledForUser(
                 optimization_guide::UserVisibleFeatureKey::kHistorySearch);
}

bool IsHistoryEmbeddingsAnswersEnabledForProfile(Profile* profile) {
  if (!IsHistoryEmbeddingsAnswersFeatureEnabled()) {
    return false;
  }

  if (optimization_guide::
          GetGenAILocalFoundationalModelEnterprisePolicySettings(
              g_browser_process->local_state()) ==
      optimization_guide::model_execution::prefs::
          GenAILocalFoundationalModelEnterprisePolicySettings::kDisallowed) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service
             ->ShouldFeatureAllowModelExecutionForSignedInUser(
                 optimization_guide::UserVisibleFeatureKey::kHistorySearch);
}

bool IsHistoryEmbeddingsSettingVisible(Profile* profile) {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service->IsSettingVisible(
             optimization_guide::UserVisibleFeatureKey::kHistorySearch);
}

bool IsHistoryEmbeddingsAnswersSettingVisible(Profile* profile) {
  if (!IsHistoryEmbeddingsAnswersFeatureEnabled()) {
    return false;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide_keyed_service &&
         optimization_guide_keyed_service
             ->ShouldModelExecutionBeAllowedForUser();
}

void PopulateSourceForWebUI(content::WebUIDataSource* source,
                            Profile* profile) {
  source->AddBoolean("enableHistoryEmbeddingsAnswers",
                     IsHistoryEmbeddingsAnswersEnabledForProfile(profile));
  source->AddBoolean(
      "enableHistoryEmbeddingsImages",
      history_embeddings::GetFeatureParameters().enable_images_for_results);
  static constexpr webui::LocalizedString kHistoryEmbeddingsStrings[] = {
      {"foundSearchResults", IDS_HISTORY_FOUND_SEARCH_RESULTS},
      {"historyEmbeddingsSearchPrompt", IDS_HISTORY_EMBEDDINGS_SEARCH_PROMPT},
      {"historyEmbeddingsHeading", IDS_HISTORY_EMBEDDINGS_HEADING},
      {"historyEmbeddingsWithAnswersResultsHeading",
       IDS_HISTORY_EMBEDDINGS_WITH_ANSWERS_RESULTS_HEADING},
      {"historyEmbeddingsHeadingLoading",
       IDS_HISTORY_EMBEDDINGS_HEADING_LOADING},
      {"historyEmbeddingsFooter", IDS_HISTORY_EMBEDDINGS_FOOTER},
      {"learnMore", IDS_LEARN_MORE},
      {"thumbsUp", IDS_THUMBS_UP_RESULTS_A11Y_LABEL},
      {"thumbsDown", IDS_THUMBS_DOWN_OPENS_FEEDBACK_FORM_A11Y_LABEL},
      {"historyEmbeddingsAnswerHeading", IDS_HISTORY_EMBEDDINGS_ANSWER_HEADING},
      {"historyEmbeddingsAnswerLoadingHeading",
       IDS_HISTORY_EMBEDDINGS_ANSWER_LOADING_HEADING},
      {"historyEmbeddingsAnswerSourceDate",
       IDS_HISTORY_EMBEDDINGS_ANSWER_SOURCE_VISIT_DATE_LABEL},
      {"historyEmbeddingsAnswererErrorModelUnavailable",
       IDS_HISTORY_EMBEDDINGS_ANSWERER_ERROR_MODEL_UNAVAILABLE},
      {"historyEmbeddingsAnswererErrorTryAgain",
       IDS_HISTORY_EMBEDDINGS_ANSWERER_ERROR_TRY_AGAIN},
      {"historyEmbeddingsMatch", IDS_HISTORY_SEARCH_EMBEDDINGS_MATCH_RESULT},
      {"historyEmbeddingsMatches", IDS_HISTORY_SEARCH_EMBEDDINGS_MATCH_RESULTS},
  };
  source->AddLocalizedStrings(kHistoryEmbeddingsStrings);
  source->AddInteger("historyEmbeddingsSearchMinimumWordCount",
                     history_embeddings::GetFeatureParameters()
                         .search_query_minimum_word_count);
  source->AddString("historyEmbeddingsSettingsUrl",
                    chrome::kHistorySearchSettingURL);

  bool logging_disabled_by_enterprise =
      profile->GetPrefs()->GetInteger(
          optimization_guide::prefs::kHistorySearchEnterprisePolicyAllowed) ==
      static_cast<int>(
          optimization_guide::model_execution::prefs::
              ModelExecutionEnterprisePolicyValue::kAllowWithoutLogging);
  if (logging_disabled_by_enterprise) {
    source->AddLocalizedString("historyEmbeddingsDisclaimer",
                               IDS_HISTORY_EMBEDDINGS_DISCLAIMER_LOGGING_OFF);
  } else {
    source->AddLocalizedString("historyEmbeddingsDisclaimer",
                               IDS_HISTORY_EMBEDDINGS_DISCLAIMER);
  }
}

bool IsHistoryEmbeddingsFeatureEnabled() {
  return false;
}

bool IsHistoryEmbeddingsAnswersFeatureEnabled() {
  if (!IsHistoryEmbeddingsFeatureEnabled()) {
    return false;
  }
  // If the feature is overridden manually or via Finch, return its value.
  if (base::FeatureList::GetStateIfOverridden(kHistoryEmbeddingsAnswers)
          .has_value()) {
    return base::FeatureList::IsEnabled(kHistoryEmbeddingsAnswers);
  }

  return false;
}

}  // namespace history_embeddings
