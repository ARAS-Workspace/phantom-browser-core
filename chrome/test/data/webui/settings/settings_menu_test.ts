// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the settings menu. */

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsMenuElement, SettingsRoutes} from 'chrome://settings/settings.js';
import {resetRouterForTesting, loadTimeData, MetricsBrowserProxyImpl, resetPageVisibilityForTesting, Router} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

// clang-format on

suite('SettingsMenu', function() {
  let settingsMenu: SettingsMenuElement;
  let routes: SettingsRoutes;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  function createSettingsMenu() {
    routes = Router.getInstance().getRoutes();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsMenu = document.createElement('settings-menu');
    document.body.appendChild(settingsMenu);
    flush();
  }

  setup(function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);
    createSettingsMenu();
  });

  teardown(function() {
    resetPageVisibilityForTesting();
  });

  // Test that navigating via the paper menu always clears the current
  // search URL parameter.
  test('clearsUrlSearchParam', async () => {
    const urlParams = new URLSearchParams('search=foo');
    Router.getInstance().navigateTo(
        Router.getInstance().getRoutes().BASIC, urlParams);
    assertEquals(
        urlParams.toString(),
        Router.getInstance().getQueryParameters().toString());

    const selector = settingsMenu.$.menu;
    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    settingsMenu.$.people.click();
    const event = await whenIronSelect;

    assertEquals(settingsMenu.$.people, event.detail.item);
    assertEquals('', Router.getInstance().getQueryParameters().toString());
  });

  test('openResetSection', function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());
  });

  test('navigateToAnotherSection', async function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());

    const whenIronSelect = eventToPromise<CustomEvent<{item: HTMLElement}>>(
        'iron-select', selector);
    Router.getInstance().navigateTo(routes.PEOPLE);
    const event = await whenIronSelect;

    assertTrue(!!selector.selected);
    assertEquals(settingsMenu.$.people, event.detail.item);
    assertEquals('/people', selector.selected.toString());
  });

  test('navigateToBasic', async function() {
    Router.getInstance().navigateTo(routes.RESET);
    const selector = settingsMenu.$.menu;
    assertTrue(!!selector.selected);
    assertEquals('/reset', selector.selected.toString());

    Router.getInstance().navigateTo(routes.BASIC);
    await microtasksFinished();
    assertFalse(!!selector.selected);
  });

  test('pageVisibility', function() {
    function assertPagesHidden(expectedHidden: boolean) {
      const ids = [
        'appearance',
        // <if expr="not is_chromeos">
        'defaultBrowser',
        // </if>
        'downloads', 'languages', 'onStartup', 'people', 'performance', 'reset',
        // <if expr="not is_chromeos">
        'system',
        // </if>
      ];

      for (const id of ids) {
        assertEquals(
            expectedHidden,
            settingsMenu.shadowRoot!.querySelector<HTMLElement>(
                                        `#${id}`)!.hidden);
      }
    }

    // The default pageVisibility should not cause menu items to be hidden.
    assertPagesHidden(false);

    // Set the visibility of the pages under test to "false".
    resetPageVisibilityForTesting({
      appearance: false,
      defaultBrowser: false,
      downloads: false,
      languages: false,
      onStartup: false,
      people: false,
      performance: false,
      reset: false,
      system: false,
    });
    createSettingsMenu();

    // Now, the menu items should be hidden.
    assertPagesHidden(true);
  });

  test('navMenuItemClickActions', async function() {
    loadTimeData.overrideValues({
      isGuest: false,
    });
    resetRouterForTesting();
    createSettingsMenu();
    await microtasksFinished();

    const testCases = [
      {
        selector: '#people',
        action: 'SettingsMenu_PeopleClicked',
        route: routes.PEOPLE,
      },
      {
        selector: '#privacy',
        action: 'SettingsMenu_PrivacyClicked',
        route: routes.PRIVACY,
      },
      {
        selector: '#performance',
        action: 'SettingsMenu_PerformanceClicked',
        route: routes.PERFORMANCE,
      },
      {
        selector: '#appearance',
        action: 'SettingsMenu_AppearanceClicked',
        route: routes.APPEARANCE,
      },
      {
        selector: '#search',
        action: 'SettingsMenu_SearchClicked',
        route: routes.SEARCH,
      },
      {
        selector: '#onStartup',
        action: 'SettingsMenu_OnStartupClicked',
        route: routes.ON_STARTUP,
      },
      {
        selector: '#languages',
        action: 'SettingsMenu_LanguagesClicked',
        route: routes.LANGUAGES,
      },
      {
        selector: '#downloads',
        action: 'SettingsMenu_DownloadsClicked',
        route: routes.DOWNLOADS,
      },
      {
        selector: '#reset',
        action: 'SettingsMenu_ResetClicked',
        route: routes.RESET,
      },
      {
        selector: '#about-menu',
        action: 'SettingsMenu_AboutClicked',
        route: routes.ABOUT,
      },
      // <if expr="not is_chromeos">
      {
        selector: '#defaultBrowser',
        action: 'SettingsMenu_DefaultBrowserClicked',
        route: routes.DEFAULT_BROWSER,
      },
      {
        selector: '#system',
        action: 'SettingsMenu_SystemClicked',
        route: routes.SYSTEM,
      },
      // </if>
    ];

    for (const testCase of testCases) {
      metricsBrowserProxy.resetResolver('recordAction');

      const navItem = settingsMenu.shadowRoot!.querySelector<HTMLElement>(
          testCase.selector);
      assertTrue(!!navItem);
      navItem.click();

      const action = await metricsBrowserProxy.whenCalled('recordAction');
      assertEquals(testCase.action, action);
      assertEquals(testCase.route, Router.getInstance().getCurrentRoute());
    }
  });
});
