// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey_registry.h"

#include <utility>

#include "chrome/browser/browsing_data/browsing_data_important_sites_util.h"
#include "chrome/browser/metrics/critical_user_journeys/critical_user_journey.h"
#include "chrome/browser/metrics/critical_user_journeys/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "ui/base/interaction/interaction_sequence.h"

namespace metrics {

CriticalUserJourneyRegistry::CriticalUserJourneyRegistry() = default;
CriticalUserJourneyRegistry::~CriticalUserJourneyRegistry() = default;

void CriticalUserJourneyRegistry::AddJourneys() {
  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileJourneySteps::kFinishDownload)
          .AddAnyOf(
              {Branch(kToolbarDownloadBubbleElementId,
                      ui::InteractionSequence::StepType::kShown,
                      ViewDownloadedFileJourneySteps::kDownloadBubbleAppears),
               Branch(
                   kToolbarDownloadButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kUserClickedDownloadBubble)})
          .AddStep(kDownloadBubbleOpenButtonId,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileJourneySteps::kClickDownloadedFile)
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kViewDownloadedFileFromAppMenuJourney)
          .AddStep(kDownloadEndedCustomEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ViewDownloadedFileFromAppMenuJourneySteps::kFinishDownload)
          .AddStep(
              kToolbarAppMenuButtonElementId,
              ui::InteractionSequence::StepType::kActivated,
              ViewDownloadedFileFromAppMenuJourneySteps::kAppMenuButtonClicked)
          .AddStep(AppMenuModel::kDownloadsMenuItem,
                   ui::InteractionSequence::StepType::kActivated,
                   ViewDownloadedFileFromAppMenuJourneySteps::
                       kDownloadsMenuItemClicked)
          .AddStep(
              kDownloadedFileOpenedCustomEventId,
              ui::InteractionSequence::StepType::kCustomEvent,
              ViewDownloadedFileFromAppMenuJourneySteps::kDownloadedFileClicked)
          .Build());

  AddJourney(
      metrics::CriticalUserJourney::Builder(&kPinExtensionJourney)
          .AddStep(kExtensionsMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   PinExtensionJourneySteps::kExtensionsMenuButtonClicked)
          .AddAnyOf(
              {Branch(kExtensionsMenuPinExtensionsEventId,
                      PinExtensionJourneySteps::kPinExtensionsViaMenuItem),
               Branch(kExtensionsSidePanelPinExtensionsEventId,
                      PinExtensionJourneySteps::
                          kPinExtensionsViaSidePanelPinButton)})
          .Build());

  AddJourney(
      CriticalUserJourney::Builder(&kClearBrowsingHistoryJourney)
          .AddAnyOf(
              {Branch(browsing_data_important_sites_util::
                          kOpenClearBrowsingDataDialogViaAcceleratorEventId,
                      ClearBrowsingHistoryJourneySteps::
                          kOpenClearBrowsingDataDialogViaAccelerator),
               Branch(
                   kToolbarAppMenuButtonElementId,
                   ui::InteractionSequence::StepType::kActivated,
                   ClearBrowsingHistoryJourneySteps::kActivateAppMenuButton)})
          .AddStep(browsing_data_important_sites_util::
                       kShowClearBrowsingDataDialogEventId,
                   ui::InteractionSequence::StepType::kCustomEvent,
                   ClearBrowsingHistoryJourneySteps::
                       kShowClearBrowsingDataSettingsDialog)
          .AddStep(
              browsing_data_important_sites_util::
                  kClearBrowsingDataHistoryEventId,
              ui::InteractionSequence::StepType::kCustomEvent,
              ClearBrowsingHistoryJourneySteps::kClearBrowsingDataHistoryEvent)
          .Build());
}

void CriticalUserJourneyRegistry::AddJourney(
    std::unique_ptr<CriticalUserJourney> journey) {
  journeys_.push_back(std::move(journey));
}

}  // namespace metrics
