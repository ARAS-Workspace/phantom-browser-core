// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';

import {omniboxPopupBrowserProxyFactory, OmniboxPopupPageHandlerRemote, SearchboxBrowserProxy} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import type {OmniboxPopupAppElement} from 'chrome://omnibox-popup.top-chrome/omnibox_popup.js';
import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {RenderType, SelectionDirection, SelectionLineState, SelectionStep, SideType} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {WindowOpenDisposition} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {$$, eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('AppTest', function() {
  let app: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;
  let handler: TestMock<OmniboxPopupPageHandlerRemote>&
      OmniboxPopupPageHandlerRemote;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      hideClassicContextButton: false,
      composeboxShowContextMenuDescription: false,
      omniboxShowContextButtonSuggestionLabel: false,
      addContext: 'Add tabs and more',
      contextButtonShapeIsOblong: false,
      composeboxShowLensIcon: false,
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    handler = TestMock.fromClass(OmniboxPopupPageHandlerRemote);
    const {instance} = omniboxPopupBrowserProxyFactory.createForTest(handler);
    omniboxPopupBrowserProxyFactory.setInstance(instance);

    app = document.createElement('omnibox-popup-app');
    document.body.appendChild(app);

    await microtasksFinished();
  });

  test('ContextMenuPrevented', async function() {
    const whenFired = eventToPromise('contextmenu', document.documentElement);
    document.documentElement.dispatchEvent(
        new Event('contextmenu', {cancelable: true}));
    const e = await whenFired;
    assertTrue(e.defaultPrevented);
  });

  test('OnlyShowsDropdownIfVisibleMatches', async () => {
    // Set autocomplete result with one visible match.
    const shownResult = createAutocompleteResultForTesting({
      matches: [
        createSearchMatchForTesting({isHidden: false}),
        createSearchMatchForTesting({isHidden: true}),
      ],
    });
    testProxy.page.autocompleteResultChanged(shownResult);
    await microtasksFinished();

    // Ensure dropdown shows.
    assertTrue(isVisible(app.getDropdown()));

    // Set autocomplete result with no visible matches.
    const hiddenResult = createAutocompleteResultForTesting({
      matches: [
        createSearchMatchForTesting({isHidden: true}),
        createSearchMatchForTesting({isHidden: true}),
      ],
    });
    testProxy.page.autocompleteResultChanged(hiddenResult);
    await microtasksFinished();

    // Ensure dropdown hides.
    assertFalse(isVisible(app.getDropdown()));

    // Force dropdown to show again.
    testProxy.page.autocompleteResultChanged(shownResult);
    await microtasksFinished();
    assertTrue(isVisible(app.getDropdown()));

    // Set autocomplete result with no matches.
    const noResult = createAutocompleteResultForTesting({matches: []});
    testProxy.page.autocompleteResultChanged(noResult);
    await microtasksFinished();

    // Ensure dropdown hides.
    assertFalse(isVisible(app.getDropdown()));
  });

  test('SecondarySideShows', async () => {
    // Ensure `canShowSecondarySide` is set to true.
    app.canShowSecondarySide = true;
    await microtasksFinished();

    const matches = [
      createSearchMatchForTesting({suggestionGroupId: 1}),
      createSearchMatchForTesting({suggestionGroupId: 100}),
    ];
    const suggestionGroupsMap = {
      1: {
        header: 'Primary',
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kDefaultPrimary,
      },
      100: {
        header: 'Secondary',
        renderType: RenderType.kDefaultVertical,
        sideType: SideType.kSecondary,
      },
    };

    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          input: 'test',
          matches: matches,
          suggestionGroupsMap: suggestionGroupsMap,
        }));
    await microtasksFinished();

    assertTrue(app.hasSecondarySide);

    // Verify `secondary-side` element is rendered and visible.
    const dropdown = $$(app, 'cr-searchbox-dropdown');
    assertTrue(!!dropdown);
    assertTrue(isVisible($$(dropdown, '.secondary-side')));

    // Verify secondary side is hidden when `canShowSecondarySide` is false.
    app.canShowSecondarySide = false;
    await microtasksFinished();
    assertFalse(isVisible($$(dropdown, '.secondary-side')));
  });

});

suite('AppTestSelectionControl', () => {
  let localApp: OmniboxPopupAppElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      omniboxShowContextButtonSuggestionLabel: false,
      webuiOmniboxPopupSelectionControlEnabled: true,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);

    localApp = document.createElement('omnibox-popup-app');
    document.body.appendChild(localApp);
    testProxy.initVisibilityPrefs();
    testProxy.page.autocompleteResultChanged(
        createAutocompleteResultForTesting({
          matches: [
            createSearchMatchForTesting({contents: 'a'}),
            createSearchMatchForTesting(
                {contents: 'b', supportsDeletion: true}),
            createSearchMatchForTesting({contents: 'c'}),
          ],
        }));
    return microtasksFinished();
  });

  test('StepSelection', async () => {
    // Starts as if omnibox just focused, with default selection (none) so
    // first step is onto first line.
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kStateOrLine);
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kStateOrLine);
    testProxy.page.openCurrentSelection(WindowOpenDisposition.CURRENT_TAB);
    const [_sequenceId, selection, disposition] =
        await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(WindowOpenDisposition.CURRENT_TAB, disposition);
    assertDeepEquals(
        {
          line: 1,
          state: SelectionLineState.kFocusedButtonRemoveSuggestion,
          actionIndex: 0,
        },
        selection);
  });

  test('OpenCurrentSelection', async () => {
    testProxy.page.stepSelection(
        SelectionDirection.kForward, SelectionStep.kAllLines);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kWholeLine);
    testProxy.page.stepSelection(
        SelectionDirection.kBackward, SelectionStep.kWholeLine);
    testProxy.page.openCurrentSelection(WindowOpenDisposition.CURRENT_TAB);
    const [_sequenceId, selection, disposition] =
        await testProxy.handler.whenCalled('openPopupSelection');
    assertEquals(WindowOpenDisposition.CURRENT_TAB, disposition);
    assertDeepEquals(
        {
          line: 0,
          state: SelectionLineState.kNormal,
          actionIndex: 0,
        },
        selection);
  });
});
