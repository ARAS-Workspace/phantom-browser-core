// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://new-tab-page/new_tab_page.js';

import type {NtpSearchboxElement} from 'chrome://new-tab-page/new_tab_page.js';
import {BrowserProxyImpl, MetricsReporterImpl, SearchboxBrowserProxy} from 'chrome://new-tab-page/new_tab_page.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestSearchboxBrowserProxy} from 'chrome://webui-test/cr_components/searchbox/test_searchbox_browser_proxy.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createAndAppendRealbox(properties: Partial<NtpSearchboxElement> = {}):
    NtpSearchboxElement {
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  const realbox = document.createElement('ntp-searchbox');
  Object.assign(realbox, properties);
  document.body.appendChild(realbox);
  return realbox;
}

suite('NewTabPageRealboxNextTest', () => {
  let realbox: NtpSearchboxElement;
  let testProxy: TestSearchboxBrowserProxy;
  let metrics: MetricsTracker;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isLensSearchbox: false,
      reportMetrics: true,
      searchboxCyclingPlaceholders: false,
      searchboxDefaultIcon: 'search.svg',
      searchboxLensSearch: true,
      searchboxSeparator: ' - ',
      searchboxVoiceSearch: true,
      energyEffectAnimationEnabled: false,
    });
  });

  setup(async () => {
    // Set up Realbox's browser proxy.
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    // Set up MetricsReporter's browser proxy.
    const testMetricsReporterProxy = TestMock.fromClass(BrowserProxyImpl);
    testMetricsReporterProxy.reset();
    const metricsReporterCallbackRouter = new PageMetricsCallbackRouter();
    testMetricsReporterProxy.setResultFor(
        'getCallbackRouter', metricsReporterCallbackRouter);
    testMetricsReporterProxy.setResultFor('getMark', Promise.resolve(null));
    BrowserProxyImpl.setInstance(testMetricsReporterProxy);
    MetricsReporterImpl.setInstanceForTest(new MetricsReporterImpl());
    metrics = fakeMetricsPrivate();
    window.open = () => null;
    realbox = createAndAppendRealbox({
      ntpRealboxNextEnabled: true,
      searchboxLayoutMode: 'Compact',
    });
    await microtasksFinished();
  });

  test('pasting text affects preventInlineAutocomplete', async () => {
    // Re-create realbox to pick up new loadTimeData.
    realbox = await createAndAppendRealbox({ntpRealboxNextEnabled: true});
    const dataTransfer = new DataTransfer();
    dataTransfer.setData('text/plain', 'hello');
    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: dataTransfer,
      bubbles: true,
      cancelable: true,
      composed: true,
    });

    realbox.$.input.inputElement.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertFalse(pasteEvent.defaultPrevented);
    assertTrue(realbox.$.input.preventInlineAutocomplete(''));
    assertEquals(1, metrics.count('NewTabPage.Realbox.Paste', 1));
    assertEquals(1, metrics.count('NewTabPage.Realbox.Paste', 0));
  });

  test('pasting into realbox records NewTabPage.Realbox.Paste', async () => {
    realbox = await createAndAppendRealbox();
    assertEquals(0, metrics.count('NewTabPage.Realbox.Paste'));

    const pasteEvent = new ClipboardEvent('paste', {
      clipboardData: new DataTransfer(),
      bubbles: true,
      cancelable: true,
      composed: true,
    });
    realbox.$.input.inputElement.dispatchEvent(pasteEvent);
    await microtasksFinished();

    assertEquals(1, metrics.count('NewTabPage.Realbox.Paste', 1));
    assertEquals(1, metrics.count('NewTabPage.Realbox.Paste', 0));
  });

  test('tabbing with inline autocompletion', async () => {
    realbox.$.input.focus();
    assertEquals(realbox.$.input, realbox.shadowRoot.activeElement);

    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    const matches = [createSearchMatchForTesting({
      allowedToBeDefaultMatch: true,
      inlineAutocompletion: 'gle',
    })];

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: realbox.activeQueryId,
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    await microtasksFinished();
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

    let start = realbox.$.input.inputElement.selectionStart!;
    let end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(
        'gle', realbox.$.input.inputElement.value.substring(start, end));

    // Tab key accepts the inline autocompletion, moves the cursor to the end,
    // and re-queries the autocomplete with the full text.
    const tabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
    });
    realbox.$.inputWrapper.dispatchEvent(tabEvent);
    assertTrue(tabEvent.defaultPrevented, 'default prevented');

    assertEquals('google', realbox.$.input.inputElement.value);
    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals(realbox.$.input.inputElement.value.length, start);

    // Shift+Tab clears inline autocompletion without triggering a new query.
    realbox.$.input.inputElement.value = 'goo';
    realbox.$.input.inputElement.dispatchEvent(new InputEvent('input'));
    await microtasksFinished();

    testProxy.callbackRouterRemote.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          queryId: realbox.activeQueryId,
          input: realbox.$.input.inputElement.value.trimStart(),
          matches: matches,
        }));
    await microtasksFinished();
    assertEquals('google', realbox.$.input.inputElement.value, 'input value');

    const shiftTabEvent = new KeyboardEvent('keydown', {
      bubbles: true,
      cancelable: true,
      composed: true,
      key: 'Tab',
      shiftKey: true,
    });
    realbox.$.inputWrapper.dispatchEvent(shiftTabEvent);

    assertEquals('goo', realbox.$.input.inputElement.value);
    assertFalse(shiftTabEvent.defaultPrevented);

    start = realbox.$.input.inputElement.selectionStart!;
    end = realbox.$.input.inputElement.selectionEnd!;
    assertEquals(start, end);
    assertEquals('goo'.length, start);
  });

  test('sets darkThemeColorsEnabled as false on search-animated-glow', () => {
    const animatedGlow =
        realbox.shadowRoot.querySelector<SearchAnimatedGlowElement>(
            'search-animated-glow');
    assertTrue(!!animatedGlow);
    assertFalse(animatedGlow.darkThemeColorsEnabled);
  });

});
