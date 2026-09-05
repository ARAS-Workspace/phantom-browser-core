// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SuggestionsFromGeminiAction, SuggestionsFromGeminiEntryPoint, YourSavedInfoDataCategory, YourSavedInfoDataChip, YourSavedInfoRelatedService} from 'chrome://settings/settings.js';
import type {AiPageComposeInteractions, AiPageHistorySearchInteractions, AiPageInteractions, AiPageSuggestionsInteractions, AutofillSettingsReferrer, DeleteBrowsingDataAction, MetricsBrowserProxy, PrivacyElementInteractions, PrivacyGuideInteractions, PrivacyGuideSettingsStates, PrivacyGuideStepsEligibleAndReached} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordBooleanHistogram',
      'recordSettingsPageHistogram',
      'recordPrivacyGuideFlowLengthHistogram',
      'recordPrivacyGuideNextNavigationHistogram',
      'recordPrivacyGuideEntryExitHistogram',
      'recordPrivacyGuideSettingsStatesHistogram',
      'recordPrivacyGuideStepsEligibleAndReachedHistogram',
      'recordDeleteBrowsingDataAction',
      // <if expr="_google_chrome and is_win">
      'recordFeatureNotificationsChange',
      // </if>
      'recordAiPageInteractions',
      'recordAiPageHistorySearchInteractions',
      'recordAiPageComposeInteractions',
      'recordAiPageSuggestionsInteractions',
      'recordAutofillSettingsReferrer',
      'recordYourSavedInfoCategoryClick',
      'recordYourSavedInfoDataChipClick',
      'recordYourSavedInfoRelatedServiceClick',
      'recordSuggestionsFromGeminiEntryPointClick',
      'recordSuggestionsFromGeminiAction',
    ]);
  }

  recordAction(action: string) {
    this.methodCalled('recordAction', action);
  }

  recordBooleanHistogram(histogramName: string, visible: boolean) {
    this.methodCalled('recordBooleanHistogram', [histogramName, visible]);
  }

  recordAutofillSettingsReferrer(
      histogramName: string, referrer: AutofillSettingsReferrer) {
    this.methodCalled(
        'recordAutofillSettingsReferrer', [histogramName, referrer]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  recordPrivacyGuideNextNavigationHistogram(
      interaction: PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideNextNavigationHistogram', interaction);
  }

  recordPrivacyGuideEntryExitHistogram(interaction: PrivacyGuideInteractions) {
    this.methodCalled('recordPrivacyGuideEntryExitHistogram', interaction);
  }

  recordPrivacyGuideSettingsStatesHistogram(state: PrivacyGuideSettingsStates) {
    this.methodCalled('recordPrivacyGuideSettingsStatesHistogram', state);
  }

  recordPrivacyGuideFlowLengthHistogram(steps: number) {
    this.methodCalled('recordPrivacyGuideFlowLengthHistogram', steps);
  }

  recordPrivacyGuideStepsEligibleAndReachedHistogram(
      status: PrivacyGuideStepsEligibleAndReached) {
    this.methodCalled(
        'recordPrivacyGuideStepsEligibleAndReachedHistogram', status);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction) {
    this.methodCalled('recordDeleteBrowsingDataAction', action);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean) {
    this.methodCalled('recordFeatureNotificationsChange', enabled);
  }
  // </if>

  recordAiPageInteractions(interaction: AiPageInteractions) {
    this.methodCalled('recordAiPageInteractions', interaction);
  }

  recordAiPageHistorySearchInteractions(
      interaction: AiPageHistorySearchInteractions) {
    this.methodCalled('recordAiPageHistorySearchInteractions', interaction);
  }

  recordAiPageComposeInteractions(interaction: AiPageComposeInteractions) {
    this.methodCalled('recordAiPageComposeInteractions', interaction);
  }

  recordAiPageSuggestionsInteractions(
      interaction: AiPageSuggestionsInteractions) {
    this.methodCalled('recordAiPageSuggestionsInteractions', interaction);
  }

  recordYourSavedInfoCategoryClick(category: YourSavedInfoDataCategory) {
    this.methodCalled('recordYourSavedInfoCategoryClick', [category]);
    if (category !== YourSavedInfoDataCategory.COUNT) {
      this.recordAction(`Settings.YourSavedInfo.CategoryClick.${
          YourSavedInfoDataCategory[category]}`);
    }
  }

  recordYourSavedInfoDataChipClick(chip: YourSavedInfoDataChip) {
    this.methodCalled('recordYourSavedInfoDataChipClick', [chip]);
    if (chip !== YourSavedInfoDataChip.COUNT) {
      this.recordAction(
          `Settings.YourSavedInfo.ChipClick.${YourSavedInfoDataChip[chip]}`);
    }
  }

  recordYourSavedInfoRelatedServiceClick(service: YourSavedInfoRelatedService) {
    this.methodCalled('recordYourSavedInfoRelatedServiceClick', [service]);
    if (service !== YourSavedInfoRelatedService.COUNT) {
      this.recordAction(`Settings.YourSavedInfo.RelatedServiceClick.${
          YourSavedInfoRelatedService[service]}`);
    }
  }

  recordSuggestionsFromGeminiEntryPointClick(
      entryPoint: SuggestionsFromGeminiEntryPoint) {
    this.methodCalled('recordSuggestionsFromGeminiEntryPointClick', entryPoint);
    if (entryPoint !== SuggestionsFromGeminiEntryPoint.COUNT) {
      const actionMap = {
        [SuggestionsFromGeminiEntryPoint.YOUR_SAVED_INFO]:
            'PersonalContext.Settings.EntryPoint.AutofillAndPasswordsSettings',
        [SuggestionsFromGeminiEntryPoint.TRAVEL]:
            'PersonalContext.Settings.EntryPoint.TravelSettings',
        [SuggestionsFromGeminiEntryPoint.SHOPPING]:
            'PersonalContext.Settings.EntryPoint.ShoppingSettings',
        [SuggestionsFromGeminiEntryPoint.IDENTITY_DOCS]:
            'PersonalContext.Settings.EntryPoint.IdentityDocsSettings',
      };
      this.recordAction(actionMap[entryPoint]);
    }
  }

  recordSuggestionsFromGeminiAction(action: SuggestionsFromGeminiAction) {
    this.methodCalled('recordSuggestionsFromGeminiAction', action);
    if (action !== SuggestionsFromGeminiAction.COUNT) {
      const actionMap = {
        [SuggestionsFromGeminiAction.MANAGE_CONNECTED_APPS_CLICK]:
            'PersonalContext.Settings.ManageConnectedAppsClick',
        [SuggestionsFromGeminiAction.TOGGLE_ON]:
            'PersonalContext.Settings.ToggledOn',
        [SuggestionsFromGeminiAction.TOGGLE_OFF]:
            'PersonalContext.Settings.ToggledOff',
      };
      this.recordAction(actionMap[action]);
    }
  }
}
