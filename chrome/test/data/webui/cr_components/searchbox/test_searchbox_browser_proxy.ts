// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WindowOpenDisposition} from '//resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import type {NavigationPredictor} from 'chrome://resources/mojo/components/omnibox/browser/omnibox.mojom-webui.js';
import type {ActionModifiers, InputMethod, OmniboxPopupSelection, PageHandlerInterface, PageRemote, PlaceholderConfig, SmartComposeStats, SuggestInventory} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {DriveDisclaimerStatus, PageCallbackRouter} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {UnguessableToken} from 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

/**
 * Helps track realbox browser call arguments. A mocked page handler remote
 * resolves the browser call promises with the arguments as an array making the
 * tests prone to change if the arguments change. This class extends the page
 * handler remote, resolving the browser call promises with named arguments.
 */
class FakePageHandler extends TestBrowserProxy implements PageHandlerInterface {
  private results_: Map<string, any> = new Map();

  constructor() {
    super([
      'activateKeyword',
      'activateMetricsFunnel',
      'clearFiles',
      'deleteAutocompleteMatch',
      'deleteContext',
      'deleteTabContext',
      'executeAction',
      'getCyclingPlaceholderConfig',
      'getDriveDisclaimerStatus',
      'getPageClassification',
      'getRecentTabs',
      'getSmartTabSharingActive',
      'getTabPreview',
      'notifySessionAbandoned',
      'notifySessionStarted',
      'onDriveDisclaimerAccepted',
      'onDriveUploadClicked',
      'onFocusChanged',
      'onNavigationLikely',
      'onThumbnailRemoved',
      'openAutocompleteMatch',
      'openPopupSelection',
      'openProfilePicker',
      'queryAutocomplete',
      'setPopupSelection',
      'setSmartComposeStats',
      'setSmartTabSharingActive',
      'startScreenshare',
      'stopAutocomplete',
      'submitQuery',
      'waitForTabFaviconLoad',
    ]);
  }

  setResultFor(methodName: string, result: any) {
    this.results_.set(methodName, result);
  }

  onFocusChanged(focused: boolean) {
    this.methodCalled('onFocusChanged', {focused});
  }

  deleteAutocompleteMatch(line: number, url: Url) {
    this.methodCalled('deleteAutocompleteMatch', {line, url});
  }

  activateKeyword(
      line: number, url: Url, matchSelectionTimestamp: TimeTicks,
      isMouseEvent: boolean) {
    this.methodCalled('activateKeyword', {
      line,
      url,
      matchSelectionTimestamp,
      isMouseEvent,
    });
  }

  executeAction(
      line: number, actionIndex: number, url: Url,
      matchSelectionTimestamp: TimeTicks, mouseButton: number, altKey: boolean,
      ctrlKey: boolean, metaKey: boolean, shiftKey: boolean) {
    this.methodCalled('executeAction', {
      line,
      actionIndex,
      url,
      matchSelectionTimestamp,
      mouseButton,
      altKey,
      ctrlKey,
      metaKey,
      shiftKey,
    });
  }

  openAutocompleteMatch(
      line: number, url: Url, areMatchesShowing: boolean, mouseButton: number,
      modifiers: ActionModifiers, viaKeyboard: boolean) {
    this.methodCalled('openAutocompleteMatch', {
      line,
      url,
      areMatchesShowing,
      mouseButton,
      modifiers,
      viaKeyboard,
    });
  }

  setSmartComposeStats(smartComposeStats: SmartComposeStats) {
    this.methodCalled('setSmartComposeStats', {smartComposeStats});
  }

  onNavigationLikely(
      line: number, url: Url, navigationPredictor: NavigationPredictor) {
    this.methodCalled('onNavigationLikely', {line, url, navigationPredictor});
  }

  onThumbnailRemoved() {
    this.methodCalled('onThumbnailRemoved', {});
  }

  queryAutocomplete(
      queryId: number, input: String16, preventInlineAutocomplete: boolean,
      cursorPosition: number, suggestInventory: SuggestInventory,
      isOnFocus: boolean, keyword: string, inputMethod: InputMethod) {
    this.methodCalled('queryAutocomplete', {
      queryId,
      input,
      preventInlineAutocomplete,
      cursorPosition,
      suggestInventory,
      isOnFocus,
      keyword,
      inputMethod,
    });
  }

  stopAutocomplete(clearResult: boolean) {
    this.methodCalled('stopAutocomplete', {clearResult});
  }

  getCyclingPlaceholderConfig(): Promise<{config: PlaceholderConfig}> {
    this.methodCalled('getCyclingPlaceholderConfig');
    return Promise.resolve({
      config: {
        texts: [],
        changeTextAnimationInterval: {microseconds: BigInt(4000) * 1000n},
        fadeTextAnimationDuration: {microseconds: BigInt(250) * 1000n},
      },
    });
  }

  getRecentTabs() {
    this.methodCalled('getRecentTabs');
    if (this.results_.has('getRecentTabs')) {
      return this.results_.get('getRecentTabs');
    }
    return Promise.resolve({tabs: []});
  }

  getTabPreview(tabId: number) {
    this.methodCalled('getTabPreview', {tabId});
    return Promise.resolve({previewDataUrl: ''});
  }

  waitForTabFaviconLoad(tabId: number) {
    this.methodCalled('waitForTabFaviconLoad', {tabId});
    return Promise.resolve({faviconDataUrl: null});
  }

  notifySessionStarted() {
    this.methodCalled('notifySessionStarted');
  }

  notifySessionAbandoned() {
    this.methodCalled('notifySessionAbandoned');
  }

  onDriveUploadClicked() {
    this.methodCalled('onDriveUploadClicked');
    if (this.results_.has('onDriveUploadClicked')) {
      return this.results_.get('onDriveUploadClicked');
    }
    return Promise.resolve({response: {files: [], error: null}});
  }

  deleteContext(fileToken: UnguessableToken) {
    this.methodCalled('deleteContext', {fileToken});
  }

  deleteTabContext(tabId: number) {
    this.methodCalled('deleteTabContext', {tabId});
  }

  clearFiles() {
    this.methodCalled('clearFiles');
  }

  submitQuery(
      queryText: string, mouseButton: number, altKey: boolean, ctrlKey: boolean,
      metaKey: boolean, shiftKey: boolean) {
    this.methodCalled(
        'submitQuery',
        {queryText, mouseButton, altKey, ctrlKey, metaKey, shiftKey});
  }

  openProfilePicker() {
    this.methodCalled('openProfilePicker');
  }

  activateMetricsFunnel(funnelName: string) {
    this.methodCalled('activateMetricsFunnel', funnelName);
  }

  setPopupSelection(selection: OmniboxPopupSelection) {
    this.methodCalled('setPopupSelection', selection);
  }

  openPopupSelection(
      resultSequenceId: number, selection: OmniboxPopupSelection,
      disposition: WindowOpenDisposition) {
    this.methodCalled(
        'openPopupSelection', {resultSequenceId, selection, disposition});
  }

  getDriveDisclaimerStatus(): Promise<{status: DriveDisclaimerStatus}> {
    this.methodCalled('getDriveDisclaimerStatus');
    if (this.results_.has('getDriveDisclaimerStatus')) {
      return this.results_.get('getDriveDisclaimerStatus');
    }
    return Promise.resolve({status: DriveDisclaimerStatus.kRestricted});
  }

  onDriveDisclaimerAccepted() {
    this.methodCalled('onDriveDisclaimerAccepted');
  }

  getPageClassification() {
    this.methodCalled('getPageClassification');
    return Promise.resolve({metricSource: 'NTP_REALBOX'});
  }

  setSmartTabSharingActive(active: boolean) {
    this.methodCalled('setSmartTabSharingActive', active);
  }

  getSmartTabSharingActive() {
    this.methodCalled('getSmartTabSharingActive');
    if (this.results_.has('getSmartTabSharingActive')) {
      return this.results_.get('getSmartTabSharingActive');
    }
    return Promise.resolve({active: false});
  }

  startScreenshare(preferEntireScreen: boolean) {
    this.methodCalled('startScreenshare', {preferEntireScreen});
    if (this.results_.has('startScreenshare')) {
      return this.results_.get('startScreenshare');
    }
    return Promise.resolve({success: true});
  }
}

export class TestSearchboxBrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.handler = new FakePageHandler();
  }
}
