// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/app.js';

import type {AppElement} from 'chrome://customize-chrome-side-panel.top-chrome/app.js';
import type {BackgroundCollection, CustomizeChromePageRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromePageCallbackRouter, CustomizeChromePageHandlerRemote, CustomizeChromeSection, NewTabPageType} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_chrome_api_proxy.js';
import {CustomizeToolbarClientCallbackRouter, CustomizeToolbarHandlerRemote} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import type {CustomizeToolbarHandlerInterface} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar.mojom-webui.js';
import {CustomizeToolbarApiProxy} from 'chrome://customize-chrome-side-panel.top-chrome/customize_toolbar/customize_toolbar_api_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {TestMock} from 'chrome://webui-test/test_mock.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {installMock} from './test_support.js';

suite('AppTest', () => {
  let customizeChromeApp: AppElement;
  let handler: TestMock<CustomizeChromePageHandlerRemote>;
  let callbackRouter: CustomizeChromePageRemote;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    handler = installMock(
        CustomizeChromePageHandlerRemote,
        (mock: CustomizeChromePageHandlerRemote) =>
            CustomizeChromeApiProxy.setInstance(
                mock, new CustomizeChromePageCallbackRouter()));
    handler.setResultFor('getBackgroundImages', new Promise(() => {}));
    handler.setResultFor('getBackgroundCollections', new Promise(() => {}));
    callbackRouter = CustomizeChromeApiProxy.getInstance()
                         .callbackRouter.$.bindNewPipeAndPassRemote();
    customizeChromeApp = document.createElement('customize-chrome-app');
    document.body.appendChild(customizeChromeApp);
    return microtasksFinished();
  });

  test('app changes pages', async () => {
    const testCollection: BackgroundCollection = {
      id: 'test',
      label: 'test',
      previewImageUrl: 'https://test.jpg',
      imageVerified: false,
    };

    // The theme pages are only shown if the attached tab allows editing the
    // theme.
    callbackRouter.setThemeEditable(true);
    await callbackRouter.$.flushForTesting();

    // Test initial page state.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(document.body, document.activeElement);

    // Send event for edit theme being clicked.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    await microtasksFinished();
    // Current page should now be categories.
    assertTrue(
        customizeChromeApp.$.categoriesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for category selected.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new CustomEvent<BackgroundCollection>(
            'collection-select', {detail: testCollection}));
    await microtasksFinished();
    // Current page should now be themes.
    assertTrue(customizeChromeApp.$.themesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.themesPage.dispatchEvent(new Event('back-click'));
    await microtasksFinished();
    // Current page should now be categories.
    assertTrue(
        customizeChromeApp.$.categoriesPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for upload image.
    customizeChromeApp.$.categoriesPage.dispatchEvent(
        new Event('local-image-upload'));
    await microtasksFinished();
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);

    // Set page back to categories.
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    await microtasksFinished();
    assertEquals(customizeChromeApp, document.activeElement);

    // Send event for back click.
    customizeChromeApp.$.categoriesPage.dispatchEvent(new Event('back-click'));
    await microtasksFinished();
    // Current page should now be overview.
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
    assertEquals(customizeChromeApp, document.activeElement);
  });

  test('app requests scroll to section update', () => {
    window.dispatchEvent(new Event('load'));
    assertGE(handler.getCallCount('updateScrollToSection'), 1);
  });

  test('app scrolls to section', async () => {
    const sections = customizeChromeApp.shadowRoot.querySelectorAll('.section');
    const sectionsScrolledTo: Element[] = [];
    sections.forEach((section) => {
      section.scrollIntoView = () => sectionsScrolledTo.push(section);
    });

    callbackRouter.setThemeEditable(true);
    await callbackRouter.$.flushForTesting();
    customizeChromeApp.$.appearanceElement.dispatchEvent(
        new Event('edit-theme-click'));
    callbackRouter.scrollToSection(CustomizeChromeSection.kAppearance);
    await callbackRouter.$.flushForTesting();

    assertEquals(1, sectionsScrolledTo.length);
    assertEquals(
        customizeChromeApp.shadowRoot.querySelector('#appearance'),
        sectionsScrolledTo[0]);
    assertTrue(
        customizeChromeApp.$.overviewPage.classList.contains('selected'));
  });

  suite('PageTransitions', () => {
    let toolbarCustomizationHandler: TestMock<CustomizeToolbarHandlerInterface>;

    suiteSetup(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      toolbarCustomizationHandler = installMock(
          CustomizeToolbarHandlerRemote,
          (mock: CustomizeToolbarHandlerRemote) =>
              CustomizeToolbarApiProxy.setInstance(
                  mock, new CustomizeToolbarClientCallbackRouter()));
      toolbarCustomizationHandler.setResultFor(
          'listActions', Promise.resolve([]));
      toolbarCustomizationHandler.setResultFor(
          'listCategories', Promise.resolve([]));
      toolbarCustomizationHandler.setResultFor(
          'getIsCustomized', Promise.resolve({customized: false}));
    });

    test(
        'page transitions back to overview when the attached tab changes',
        async () => {
          // The theme pages are only shown if the attached tab allows editing
          // the theme.
          callbackRouter.setThemeEditable(true);
          await callbackRouter.$.flushForTesting();

          // start on the overview page
          assertTrue(
              customizeChromeApp.$.overviewPage.classList.contains('selected'));
          assertEquals(document.body, document.activeElement);

          // Send event for edit theme being clicked.
          customizeChromeApp.$.appearanceElement.dispatchEvent(
              new Event('edit-theme-click'));
          await microtasksFinished();

          // Current page should now be categories.
          assertTrue(customizeChromeApp.$.categoriesPage.classList.contains(
              'selected'));
          assertEquals(customizeChromeApp, document.activeElement);

          callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
          callbackRouter.setThemeEditable(false);
          await microtasksFinished();

          assertTrue(
              customizeChromeApp.$.overviewPage.classList.contains('selected'));
          assertEquals(document.body, document.activeElement);
        });

    test(
        'toolbar page does not transition back to overview when the attached ' +
            'tab changes',
        async () => {
          assertTrue(
              !!customizeChromeApp.shadowRoot.querySelector('#toolbarButton'));

          // Send event for toolbar button being clicked.
          customizeChromeApp.shadowRoot
              .querySelector<HTMLElement>('#toolbarButton')!.click();
          await microtasksFinished();
          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot.querySelector('#toolbarPage')!
                  .classList.contains('selected'));

          callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
          await microtasksFinished();

          // Current page should now be toolbar.
          assertTrue(
              customizeChromeApp.shadowRoot.querySelector('#toolbarPage')!
                  .classList.contains('selected'));
        });
  });

  suite('Footer', () => {
    suiteSetup(() => {
      loadTimeData.overrideValues({
        'footerEnabled': true,
      });
    });

    ([
      [
        false,
        false,
        'hidden with extension policy disabled',
      ],
      [
        true,
        true,
        'visible with extension notice shown but extension policy disabled',
      ],
    ] as Array<[boolean, boolean, string]>)
        .forEach(([extensionPolicyEnabled, expected, description]) => {
          test(`toogle should be ${description}`, async () => {
            await Promise.all([
              handler.whenCalled('updateFooterSettings'),
              handler.whenCalled('updateAttachedTabState'),
            ]);
            callbackRouter.setFooterSettings(true, extensionPolicyEnabled);
            callbackRouter.attachedTabStateUpdated(NewTabPageType.kExtension);
            await callbackRouter.$.flushForTesting();
            assertEquals(
                expected,
                !!customizeChromeApp.shadowRoot.querySelector('#footer'));
          });
        });
  });
});
