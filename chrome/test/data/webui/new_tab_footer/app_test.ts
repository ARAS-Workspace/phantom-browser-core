// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {NewTabFooterAppElement} from 'chrome://newtab-footer/app.js';
import {FooterCustomizeChromeEntryPoint, FooterElement} from 'chrome://newtab-footer/app.js';
import type {CustomizeButtonsDocumentRemote} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {browserProxyFactory as customizeButtonsproxyFactory, CustomizeButtonsHandlerRemote, SidePanelOpenTrigger} from 'chrome://newtab-footer/customize_buttons.mojom-webui.js';
import {CustomizeChromeSection} from 'chrome://newtab-footer/customize_chrome.mojom-webui.js';
import type {NewTabFooterDocumentRemote} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import {browserProxyFactory as newTabFooterproxyFactory, NewTabFooterHandlerRemote, NewTabPageType} from 'chrome://newtab-footer/new_tab_footer.mojom-webui.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('NewTabFooterAppTest', () => {
  let element: NewTabFooterAppElement;
  let handler: TestMock<NewTabFooterHandlerRemote>&NewTabFooterHandlerRemote;
  let callbackRouter: NewTabFooterDocumentRemote;
  let customizeButtonsCallbackRouterRemote: CustomizeButtonsDocumentRemote;
  let customizeButtonsHandler: TestMock<CustomizeButtonsHandlerRemote>&
      CustomizeButtonsHandlerRemote;
  let metrics: MetricsTracker;

  async function setupFooter(ntpType: NewTabPageType = NewTabPageType.kOther) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = TestMock.fromClass(NewTabFooterHandlerRemote);
    const {instance, remote} = newTabFooterproxyFactory.createForTest(handler);
    callbackRouter = remote;
    newTabFooterproxyFactory.setInstance(instance);

    customizeButtonsHandler = TestMock.fromClass(CustomizeButtonsHandlerRemote);
    const {instance: customizeInstance, remote: customizeRemote} =
        customizeButtonsproxyFactory.createForTest(customizeButtonsHandler);
    customizeButtonsCallbackRouterRemote = customizeRemote;
    customizeButtonsproxyFactory.setInstance(customizeInstance);
    metrics = fakeMetricsPrivate();

    element = document.createElement('new-tab-footer-app');
    document.body.appendChild(element);
    callbackRouter.attachedTabStateUpdated(
        ntpType, /*canCustomizeChrome=*/ true);
    await callbackRouter.$.flushForTesting();
    await microtasksFinished();
  }

  suite('Extension', () => {
    setup(async () => {
      await setupFooter(NewTabPageType.kExtension);
    });

    test('Get extension name on initialization', async () => {
      // Act.
      const fooName = 'foo';
      callbackRouter.setNtpExtensionName(fooName);
      await callbackRouter.$.flushForTesting();

      // Assert.
      let name = $$(element, '#extensionNameContainer');
      assertTrue(!!name);
      const link = name.querySelector<HTMLElement>('[role="link"]');
      assertTrue(!!link);
      assertEquals(link.innerText, fooName);

      // Act.
      callbackRouter.setNtpExtensionName('');
      await callbackRouter.$.flushForTesting();

      // Assert.
      name = $$(element, '#extensionNameContainer');
      assertFalse(!!name);
    });

    test('Click extension name link', async () => {
      // Arrange.
      callbackRouter.setNtpExtensionName('foo');
      await callbackRouter.$.flushForTesting();

      // Act.
      const link = $$(element, '#extensionNameContainer [role="link"]');
      assertTrue(!!link);
      link.click();

      // Assert.
      assertEquals(
          1, handler.getCallCount('openExtensionOptionsPageWithFallback'));
      assertEquals(1, metrics.count('NewTabPage.Footer.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.Click', FooterElement.EXTENSION_NAME));
    });

    ([
      [NewTabPageType.kExtension, true],
      [NewTabPageType.kOther, false],
    ] as Array<[NewTabPageType, boolean]>)
        .forEach(([pageType, expected]) => {
          test(`Change NTP type to ${pageType}`, async () => {
            // Act.
            const fooName = 'foo';
            callbackRouter.attachedTabStateUpdated(
                pageType, /*canCustomizeChrome=*/ true);
            callbackRouter.setNtpExtensionName(fooName);
            await callbackRouter.$.flushForTesting();

            // Assert.
            const name = $$(element, '#extensionNameContainer');
            assertEquals(expected, !!name);
          });
        });
  });

  suite('CustomizeChromeButton', () => {
    setup(async () => {
      await setupFooter(NewTabPageType.kExtension);
    });

    function getCustomizeButton(): CrButtonElement {
      const buttons = $$(element, '#customizeButtons');
      assertTrue(!!buttons);
      const button = $$<CrButtonElement>(buttons, '#customizeButton');
      assertTrue(!!button);
      return button;
    }

    test('clicking customize button opens side panel', () => {
      // Act.
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            true,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabFooter,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
    });

    test(`clicking #customizeButton records click`, () => {
      getCustomizeButton().click();
      assertEquals(1, metrics.count('NewTabPage.Footer.Click'));
      assertEquals(
          1,
          metrics.count(
              'NewTabPage.Footer.Click', FooterElement.CUSTOMIZE_BUTTON));
    });

    test('clicking customize button hides side panel', async () => {
      // Act.
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      getCustomizeButton().click();

      // Assert.
      assertDeepEquals(
          [
            false,
            CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabFooter,
          ],
          customizeButtonsHandler.getArgs(
              'setCustomizeChromeSidePanelVisible')[0]);
      assertEquals(
          0,
          metrics.count(
              'NewTabPage.Footer.CustomizeChromeOpened',
              FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON));
    });

    test('clicking customize button is accessible', async () => {
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(true);
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      assertEquals('true', getCustomizeButton().getAttribute('aria-pressed'));
      customizeButtonsCallbackRouterRemote
          .setCustomizeChromeSidePanelVisibility(false);
      await customizeButtonsCallbackRouterRemote.$.flushForTesting();
      assertEquals('false', getCustomizeButton().getAttribute('aria-pressed'));
    });

    ([
      [NewTabPageType.kOther, false],
      [NewTabPageType.kExtension, true],
    ] as Array<[NewTabPageType, boolean]>)
        .forEach(([pageType, expected]) => {
          test(
              `setting ntp type ${pageType} shows customize button ${expected}`,
              async () => {
                // Act.
                callbackRouter.attachedTabStateUpdated(
                    pageType, /*canCustomizeChrome=*/ true);
                await callbackRouter.$.flushForTesting();

                // Assert.
                const buttons = $$(element, '#customizeButtons');
                assertEquals(!!buttons, expected);
              });
        });

    test('button hidden when `canCustomizeChrome` is false', async () => {
      // Arrange.
      const button = getCustomizeButton();
      assertTrue(isVisible(button));

      // Act.
      callbackRouter.attachedTabStateUpdated(
          NewTabPageType.kOther, /*canCustomizeChrome=*/ false);
      await callbackRouter.$.flushForTesting();
      callbackRouter.attachedTabStateUpdated(
          NewTabPageType.kExtension, /*canCustomizeChrome=*/ false);
      await callbackRouter.$.flushForTesting();

      // Assert.
      const buttons = $$(element, '#customizeButtons');
      assertFalse(!!buttons);
    });
  });

  suite('Misc', () => {
    setup(async () => {
      await setupFooter();
    });

    test(`right click opens context menu`, async () => {
      const container = $$(element, '#container');
      assertTrue(!!container);

      container.dispatchEvent(new MouseEvent('contextmenu'));

      await handler.whenCalled('showContextMenu');
      assertEquals(
          1,
          metrics.count('NewTabPage.Footer.Click', FooterElement.CONTEXT_MENU));
    });
  });
});
