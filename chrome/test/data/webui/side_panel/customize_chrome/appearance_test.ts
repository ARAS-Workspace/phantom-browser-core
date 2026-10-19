// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';

import type {AppearanceElement} from 'chrome://customize-chrome-side-panel.top-chrome/appearance.js';
import {CustomizeChromeAction} from 'chrome://customize-chrome-side-panel.top-chrome/common.js';
import type {CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, NewTabPageType} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import type {HoverButtonElement} from 'chrome://customize-chrome-side-panel.top-chrome/hover_button.js';
import type {ManagedDialogElement} from 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import type {CrA11yAnnouncerMessagesSentEvent} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle, createTheme, installMock} from './test_support.js';

const newTabPageTypes = [
  NewTabPageType.kThirdPartyWebUI,
  NewTabPageType.kExtension,
  NewTabPageType.kNone,
];

suite('AppearanceTest', () => {
  let appearanceElement: AppearanceElement;
  let callbackRouterRemote: CustomizeChromePageRemote;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let metrics: MetricsTracker;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    callbackRouterRemote = CustomizeChromeApiProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
    metrics = fakeMetricsPrivate();
    appearanceElement = document.createElement('customize-chrome-appearance');
    document.body.appendChild(appearanceElement);
  });

  test('classic chrome button shows with background image', async () => {
    const theme = createTheme();
    theme.hasBackgroundImage = true;

    callbackRouterRemote.setTheme(theme);
    callbackRouterRemote.attachedTabStateUpdated(
        NewTabPageType.kThirdPartyWebUI);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(!appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button does not show with classic chrome', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    callbackRouterRemote.attachedTabStateUpdated(
        NewTabPageType.kThirdPartyWebUI);
    await callbackRouterRemote.$.flushForTesting();

    assertTrue(appearanceElement.$.setClassicChromeButton.hidden);
  });

  test('classic chrome button sets theme to classic chrome', async () => {
    const theme = createTheme();
    theme.hasBackgroundImage = true;

    callbackRouterRemote.setTheme(theme);
    callbackRouterRemote.attachedTabStateUpdated(
        NewTabPageType.kThirdPartyWebUI);
    await callbackRouterRemote.$.flushForTesting();

    appearanceElement.$.setClassicChromeButton.click();
    assertEquals(1, handler.getCallCount('removeBackgroundImage'));
    assertEquals(1, handler.getCallCount('setDefaultColor'));
  });

  test('announce default theme starting with non-default theme', async () => {
    loadTimeData.overrideValues({
      updatedToClassicChrome: 'Theme updated to Classic Chrome',
    });

    // Add listener to count announcements.
    let announcementCount = 0;
    const updateAnnouncementCount = () => {
      announcementCount += 1;
    };
    document.body.addEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
    const announcementPromise =
        eventToPromise<CrA11yAnnouncerMessagesSentEvent>(
            'cr-a11y-announcer-messages-sent', document.body);

    // Set non-classic chrome theme.
    let theme = createTheme();
    theme.hasBackgroundImage = true;

    callbackRouterRemote.setTheme(theme);
    callbackRouterRemote.attachedTabStateUpdated(
        NewTabPageType.kThirdPartyWebUI);
    await callbackRouterRemote.$.flushForTesting();

    // Set classic chrome. This should announce.
    theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    const announcement = await announcementPromise;
    assertEquals(announcementCount, 1);
    assertTrue(!!announcement);
    assertTrue(announcement.detail.messages.includes(
        'Theme updated to Classic Chrome'));

    // Cleanup event listener.
    document.body.removeEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
  });

  test('announce default theme starting with default theme', async () => {
    loadTimeData.overrideValues({
      updatedToClassicChrome: 'Theme updated to Classic Chrome',
    });

    // Add listener to count announcements.
    let announcementCount = 0;
    const updateAnnouncementCount = () => {
      announcementCount += 1;
    };
    document.body.addEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
    const announcementPromise =
        eventToPromise<CrA11yAnnouncerMessagesSentEvent>(
            'cr-a11y-announcer-messages-sent', document.body);

    // Set initial classic chrome, which should not announce, since
    // it is the initial theme.
    let theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    callbackRouterRemote.attachedTabStateUpdated(
        NewTabPageType.kThirdPartyWebUI);
    await callbackRouterRemote.$.flushForTesting();

    // Set non-classic chrome theme.
    theme = createTheme();
    theme.hasBackgroundImage = true;

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    // Set classic chrome. This should announce.
    theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();

    const announcement = await announcementPromise;
    assertEquals(announcementCount, 1);
    assertTrue(!!announcement);
    assertTrue(announcement.detail.messages.includes(
        'Theme updated to Classic Chrome'));

    // Cleanup event listener.
    document.body.removeEventListener(
        'cr-a11y-announcer-messages-sent', updateAnnouncementCount);
  });

  test(
      'do not move focus if not on classic chrome button when set',
      async () => {
        // Arrange.
        let theme = createTheme();
        theme.hasBackgroundImage = true;

        callbackRouterRemote.setTheme(theme);
        callbackRouterRemote.attachedTabStateUpdated(
            NewTabPageType.kThirdPartyWebUI);
        await callbackRouterRemote.$.flushForTesting();
        const focusedElement = appearanceElement.shadowRoot.activeElement;
        assertNotEquals(
            focusedElement, appearanceElement.$.setClassicChromeButton);

        // Act.
        theme = createTheme();

        callbackRouterRemote.setTheme(theme);
        await callbackRouterRemote.$.flushForTesting();

        // Assert.
        assertEquals(
            appearanceElement.shadowRoot.activeElement, focusedElement);
      });

  test('color picker shows when 3P theme info not set', async () => {
    const theme = createTheme();

    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    assertNotStyle(appearanceElement.$.chromeColors, 'display', 'none');
  });

  test('respects policy for reset', async () => {
    const theme = createTheme();
    theme.backgroundManagedByPolicy = true;
    callbackRouterRemote.setTheme(theme);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    appearanceElement.$.setClassicChromeButton.click();
    await microtasksFinished();

    const managedDialog =
        $$<ManagedDialogElement>(appearanceElement, 'managed-dialog');
    assertTrue(!!managedDialog);
    assertTrue(managedDialog.$.dialog.open);
    assertEquals(0, handler.getCallCount('setDefaultColor'));
    assertEquals(0, handler.getCallCount('removeBackgroundImage'));
  });

  test('shows managed name and description', async () => {
    // Arrange.
    const theme = createTheme();
    theme.hasBackgroundImage = true;
    callbackRouterRemote.setTheme(theme);
    await microtasksFinished();

    // Act.
    const name = 'foo';
    const desc = 'bar';
    callbackRouterRemote.ntpManagedByNameUpdated(name, desc);
    await callbackRouterRemote.$.flushForTesting();
    await microtasksFinished();

    const managedButton = $$<HoverButtonElement>(
        appearanceElement, '#thirdPartyManageLinkButton');
    assertTrue(!!managedButton);
    assertEquals(name, managedButton.label);
    assertEquals(desc, managedButton.labelDescription);
  });

  suite('showBottomDivider', () => {
    [true, false].forEach((showClassicChromeButton) => {
      const buttonString = `showClassicChromeButton ${showClassicChromeButton}`;

      test(`${showClassicChromeButton} when ${buttonString}`, async () => {
        const theme = createTheme();
        theme.hasBackgroundImage = showClassicChromeButton;

        callbackRouterRemote.setTheme(theme);
        callbackRouterRemote.attachedTabStateUpdated(
            NewTabPageType.kThirdPartyWebUI);
        await callbackRouterRemote.$.flushForTesting();

        assertNotEquals(
            appearanceElement.$.setClassicChromeButton.hidden,
            showClassicChromeButton);
        assertNotEquals(
            $$<HTMLElement>(appearanceElement, '.sp-hr')!.hidden,
            showClassicChromeButton);
      });
    });
  });

  suite('third party theme', () => {
    test('3P theme hides the color picker', async () => {
      const theme = createTheme();
      theme.hasThirdPartyTheme = true;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();
      assertStyle(appearanceElement.$.chromeColors, 'display', 'none');
      if (loadTimeData.getBoolean('footerEnabled')) {
        assertStyle(
            appearanceElement.$.setClassicChromeButton, 'display', 'none');
      } else {
        assertNotStyle(
            appearanceElement.$.setClassicChromeButton, 'display', 'none');
      }
    });
  });

  suite('Metrics', () => {
    test('Clicking set to classic Chrome button sets metric', async () => {
      const theme = createTheme();
      theme.hasBackgroundImage = true;

      callbackRouterRemote.setTheme(theme);
      await callbackRouterRemote.$.flushForTesting();

      appearanceElement.$.setClassicChromeButton.click();

      assertEquals(
          1, metrics.count('NewTabPage.CustomizeChromeSidePanelAction'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.CustomizeChromeSidePanelAction',
              CustomizeChromeAction.SET_CLASSIC_CHROME_THEME_CLICKED));
    });
  });

  suite('NtpFooterEnabled', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        footerEnabled: true,
      });
    });

    newTabPageTypes.forEach((t) => {
      test(`classic chrome button NTP type ${t}`, async () => {
        // Arrange.
        const theme = createTheme();
        theme.hasBackgroundImage = true;
        callbackRouterRemote.setTheme(theme);
        callbackRouterRemote.attachedTabStateUpdated(t);
        await microtasksFinished();

        // Assert.
        assertEquals(
            t === NewTabPageType.kThirdPartyWebUI,
            !appearanceElement.$.setClassicChromeButton.hidden);
      });
    });
  });
});
