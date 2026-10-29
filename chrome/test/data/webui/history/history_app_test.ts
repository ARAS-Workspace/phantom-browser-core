// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://history/history.js';

import type {HistoryAppElement} from 'chrome://history/history.js';
import {BrowserProxyImpl, CrRouter} from 'chrome://history/history.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';
import {COLORS_CSS_SELECTOR} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {TestHistoryBrowserProxy} from './test_browser_proxy.js';
// clang-format on

suite('HistoryFilterChipsVisibility', function() {
  let element: HistoryAppElement;

  setup(() => {
    const browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    // Some of the tests below assume the query state is fully reset to empty
    // between tests.
    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
  });

  teardown(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  function createPage() {
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  }

  test('FilterChipsVisible', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryActorIntegrationM3Enabled: true,
      isGlicWebActuationAvailable: true,
    });
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsNotVisible_M3Off', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryActorIntegrationM3Enabled: false,
      isGlicWebActuationAvailable: true,
    });
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertFalse(isChildVisible(element, '#historyFilterChips'));
  });

  test('FilterChipsNotVisibile_GlicActuationOff', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryActorIntegrationM3Enabled: true,
      isGlicWebActuationAvailable: false,
    });
    // Re-create the element to pick up the new loadTimeData.
    await createPage();
    assertFalse(isChildVisible(element, '#historyFilterChips'));
  });

  test('PropagatesFilterChanges', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryActorIntegrationM3Enabled: true,
      isGlicWebActuationAvailable: true,
    });
    await createPage();

    const filterChips =
        element.shadowRoot.querySelector<HTMLElement>('#historyFilterChips');
    assertTrue(!!filterChips);

    const changeQueryEventPromise = eventToPromise<CustomEvent<{
      search: string,
      includeUserVisits: boolean,
      includeActorVisits: boolean,
    }>>('change-query', element);

    filterChips.dispatchEvent(new CustomEvent('filter-changed', {
      detail: {
        userVisits: false,
        actorVisits: true,
      },
      bubbles: true,
      composed: true,
    }));

    const event = await changeQueryEventPromise;

    // The query sent to the manager includes the new filters.
    assertFalse(event.detail.includeUserVisits);
    assertTrue(event.detail.includeActorVisits);
    assertEquals('', event.detail.search);
  });

  test('HidesFilterChipsOnGroupedView', async () => {
    loadTimeData.overrideValues({
      isBrowsingHistoryActorIntegrationM3Enabled: true,
      isGlicWebActuationAvailable: true,
    });
    await createPage();

    assertTrue(isChildVisible(element, '#historyFilterChips'));

    element.$.router.selectedPage = 'grouped';
    await microtasksFinished();

    // Should be hidden for grouped view.
    assertFalse(isChildVisible(element, '#historyFilterChips'));

    element.$.router.selectedPage = 'history';
    await microtasksFinished();
    assertTrue(isChildVisible(element, '#historyFilterChips'));
  });
});

suite('WebuiRefresh2026', function() {
  const WEBUI_REFRESH_ATTR = 'webui-refresh-2026';
  let element: HistoryAppElement;
  let browserProxy: TestHistoryBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    browserProxy = new TestHistoryBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);

    window.history.replaceState({}, '', '/');
    CrRouter.resetForTesting();
  });

  function createPage() {
    element = document.createElement('history-app');
    document.body.appendChild(element);
    return microtasksFinished();
  }

  test('Enabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: WEBUI_REFRESH_ATTR});
    await createPage();

    assertNotEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });

  test('Disabled', async () => {
    loadTimeData.overrideValues({webuiRefresh2026: ''});
    await createPage();

    assertEquals(null, document.body.querySelector(COLORS_CSS_SELECTOR));
  });
});
