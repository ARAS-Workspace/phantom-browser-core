// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';

import {getContextMenuDialog, SearchboxBrowserProxy, UnboundedMenuManager, updateUnboundedElementVisibility} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {OmniboxEverywhereAppElement, OmniboxEverywhereOmniboxElement, OmniboxEverywhereProfileIconElement, UnboundedElement} from 'chrome://omnibox-everywhere.top-chrome/omnibox_everywhere.js';
import type {SearchAnimatedGlowElement} from 'chrome://resources/cr_components/search/animated_glow.js';
import {GlowAnimationState} from 'chrome://resources/cr_components/search/constants.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestSearchboxBrowserProxy} from './test_searchbox_browser_proxy.js';

suite('OmniboxEverywhereOmniboxTest', () => {
  let omnibox: OmniboxEverywhereOmniboxElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      isFuseboxEnabled: true,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
      composeboxContextDragAndDropEnabled: true,
      energyEffectAnimationEnabled: false,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      contextManagementInComposeboxEnabled: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      searchboxLayoutMode: 'TallBottomContext',
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    omnibox = document.createElement('omnibox-everywhere-omnibox');
    document.body.appendChild(omnibox);
    await microtasksFinished();
  });

  test(
      'sets is-dragging-file attribute on dragenter and removes on dragleave',
      async () => {
        const inputWrapper = omnibox.shadowRoot.querySelector('#inputWrapper');
        assertTrue(!!inputWrapper);

        assertFalse(omnibox.hasAttribute('is-dragging-file'));

        inputWrapper?.dispatchEvent(new DragEvent('dragenter', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(omnibox.hasAttribute('is-dragging-file'));
        assertEquals(GlowAnimationState.DRAGGING, omnibox.animationState);

        inputWrapper?.dispatchEvent(new DragEvent('dragleave', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(omnibox.hasAttribute('is-dragging-file'));
        assertEquals(GlowAnimationState.NONE, omnibox.animationState);
      });

  test(
      'clicking voice search button dispatches open-voice-search event',
      async () => {
        let eventFired = false;
        omnibox.addEventListener('open-voice-search', () => {
          eventFired = true;
        });

        const voiceBtn = omnibox.shadowRoot.querySelector<HTMLElement>(
            '#voiceSearchButton')!;
        assertTrue(!!voiceBtn);
        voiceBtn.click();
        await microtasksFinished();

        assertTrue(eventFired);
      });

  test(
      'configures animated glow and compose button properties correctly',
      () => {
        const glow =
            omnibox.shadowRoot.querySelector<SearchAnimatedGlowElement>(
                'search-animated-glow');
        assertTrue(!!glow);
        assertEquals('OmniboxEverywhere', glow.entrypointName);

        const composeButton =
            omnibox.shadowRoot.querySelector('#composeButton');
        assertTrue(!!composeButton);
      });

  test(
      'updates has-user-input on compose button when text changes',
      async () => {
        const composeButton =
            omnibox.shadowRoot.querySelector('#composeButton')!;
        assertTrue(!!composeButton);
        assertFalse(composeButton.hasAttribute('has-user-input'));

        const input = omnibox.shadowRoot.querySelector('#input')!;
        input.dispatchEvent(new CustomEvent('searchbox-input-text-updated', {
          detail: {value: 'test query', isComposing: false},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertTrue(composeButton.hasAttribute('has-user-input'));

        input.dispatchEvent(new CustomEvent('searchbox-input-text-updated', {
          detail: {value: '', isComposing: false},
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(composeButton.hasAttribute('has-user-input'));
      });

  test(
      'clicking compose button with query text submits query and notifies ' +
          'session',
      async () => {
        omnibox.setInputText('test query');
        await microtasksFinished();

        const composeButton =
            omnibox.shadowRoot.querySelector<HTMLElement>('#composeButton')!;
        assertTrue(!!composeButton);
        composeButton.dispatchEvent(new CustomEvent('compose-click', {
          bubbles: true,
          composed: true,
          detail: {
            button: 0,
            ctrlKey: false,
            metaKey: false,
            shiftKey: false,
          },
        }));

        await testProxy.handler.whenCalled('submitQuery');
        assertEquals(1, testProxy.handler.getCallCount('submitQuery'));
        assertEquals(1, testProxy.handler.getCallCount('notifySessionStarted'));
        assertEquals(
            1, testProxy.handler.getCallCount('activateMetricsFunnel'));
        const submitArgs = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('test query', submitArgs[0]);
        assertEquals(0, submitArgs[1]);      // button
        assertEquals(false, submitArgs[2]);  // altKey
        assertEquals(false, submitArgs[3]);  // ctrlKey
        assertEquals(false, submitArgs[4]);  // metaKey
        assertEquals(false, submitArgs[5]);  // shiftKey
        assertEquals(false, submitArgs[6]);  // isVoiceSearch
      });

  test('dropdownIsVisible preserves bottomControls', async () => {
    const bottomControls =
        omnibox.shadowRoot.querySelector<HTMLElement>('#bottomControls');
    assertTrue(!!bottomControls);
    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);

    omnibox.dropdownIsVisible = true;
    await microtasksFinished();

    assertTrue(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);

    omnibox.dropdownIsVisible = false;
    await microtasksFinished();

    assertFalse(omnibox.hasAttribute('dropdown-is-visible'));
    assertEquals('flex', window.getComputedStyle(bottomControls).display);
  });
});


suite('UnboundedUtilsTest', () => {
  setup(() => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
  });

  test('getContextMenuDialog resolves nested dialog correctly', () => {
    const host = document.createElement('div');
    const hostRoot = host.attachShadow({mode: 'open'});
    const entrypoint = document.createElement('div');
    entrypoint.id = 'context';
    const entrypointRoot = entrypoint.attachShadow({mode: 'open'});
    const menu1 = document.createElement('div');
    menu1.id = 'menu';
    const menu1Root = menu1.attachShadow({mode: 'open'});
    const menu2 = document.createElement('div');
    menu2.id = 'menu';
    const menu2Root = menu2.attachShadow({mode: 'open'});
    const dialog = document.createElement('dialog') as UnboundedElement;
    dialog.id = 'dialog';

    menu2Root.appendChild(dialog);
    menu1Root.appendChild(menu2);
    entrypointRoot.appendChild(menu1);
    hostRoot.appendChild(entrypoint);
    document.body.appendChild(host);

    assertEquals(dialog, getContextMenuDialog(hostRoot, '#context'));
    assertEquals(null, getContextMenuDialog(null, '#context'));
    assertEquals(null, getContextMenuDialog(hostRoot, '#nonExistent'));
  });

  test('UnboundedMenuManager manages lifecycle cleanly', async () => {
    const mockDialog = document.createElement('dialog') as UnboundedElement;
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(mockDialog);
    let showCalled = false;
    let hideCalled = false;
    mockDialog.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };
    mockDialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };

    const host = document.createElement('div');
    (host as unknown as {getDialog: () => HTMLDialogElement}).getDialog = () =>
        mockDialog;

    let closedFired = false;
    const manager = new UnboundedMenuManager(() => host, () => {
      closedFired = true;
    });

    assertEquals(mockDialog, manager.getDialog());
    assertFalse(manager.isDialogOpen());

    manager.onContextMenuOpened();
    assertTrue(showCalled);

    const event = new ToggleEvent('unbounded', {
      oldState: 'open',
      newState: 'closed',
    });
    mockDialog.dispatchEvent(event);
    assertTrue(closedFired);

    manager.onContextMenuClosed();
    await microtasksFinished();
    assertTrue(hideCalled);
    mockDialog.remove();
  });

  test('updateUnboundedElementVisibility show and hide', async () => {
    const dialog = document.createElement('dialog') as UnboundedElement;
    // Dialog must be attached to the DOM so Blink tracks its open state in
    // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
    document.body.appendChild(dialog);
    let showCalled = false;
    let hideCalled = false;
    dialog.showUnboundedElement = () => {
      showCalled = true;
      return Promise.resolve();
    };
    dialog.hideUnboundedElement = () => {
      hideCalled = true;
      return Promise.resolve();
    };

    updateUnboundedElementVisibility(dialog, true);
    assertTrue(dialog.hasAttribute('unbounded'));
    assertTrue(showCalled);

    updateUnboundedElementVisibility(dialog, false);
    await microtasksFinished();
    assertTrue(hideCalled);
    assertFalse(dialog.hasAttribute('unbounded'));
    dialog.remove();
  });

  test(
      'updateUnboundedElementVisibility handles async check and failures',
      async () => {
        const dialog = document.createElement('dialog') as UnboundedElement;
        // Dialog must be attached to the DOM so Blink tracks its open state in
        // AllOpenDialogs(), avoiding DCHECK failures on attribute changes.
        document.body.appendChild(dialog);
        let showCallCount = 0;
        dialog.showUnboundedElement = () => {
          showCallCount++;
          return Promise.resolve();
        };

        // Async check returns false -> should not call showUnboundedElement.
        updateUnboundedElementVisibility(dialog, true, () => false);
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertEquals(0, showCallCount);
        assertFalse(dialog.hasAttribute('unbounded'));

        // Async check returns true -> should call showUnboundedElement.
        updateUnboundedElementVisibility(dialog, true, () => true);
        await new Promise(resolve => requestAnimationFrame(resolve));
        assertEquals(1, showCallCount);

        // Hide with rejection cleans up attribute.
        dialog.setAttribute('unbounded', '');
        dialog.hideUnboundedElement = () =>
            Promise.reject(new Error('Native error'));
        updateUnboundedElementVisibility(dialog, false);
        await microtasksFinished();
        assertFalse(dialog.hasAttribute('unbounded'));
        dialog.remove();
      });
});


declare global {
  interface Window {
    webkitSpeechRecognition: unknown;
  }
}

class MockSpeechRecognition {
  start() {}
  stop() {}
  abort() {}
}

suite('OmniboxEverywhereAppTest', () => {
  let app: OmniboxEverywhereAppElement;
  let testProxy: TestSearchboxBrowserProxy;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    window.webkitSpeechRecognition = MockSpeechRecognition;

    loadTimeData.overrideValues({
      isFuseboxEnabled: true,
      searchboxVoiceSearch: true,
      searchboxLensSearch: true,
      omniboxPopupDebugEnabled: false,
      searchboxLayoutMode: 'normal',
      caretAnimationEnabled: true,
      composeboxAnimationDisabled: false,
      contextButtonShapeIsOblong: false,
      contextManagementInComposeboxEnabled: false,
      searchboxCr23Theming: true,
      searchboxCr23SteadyStateShadow: false,
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: false,
      composeboxCancelButtonTitle: 'Close AI Mode',
      composeboxCancelButtonTitleInput: 'Clear text',
    });

    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    app = document.createElement('omnibox-everywhere-app');
    document.body.appendChild(app);
    await microtasksFinished();
  });


  test('open-voice-search opens voice search dialog overlay', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const dialog =
        app.shadowRoot.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    assertTrue(!!dialog);
    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch');
    assertTrue(!!voiceSearch);
  });

  test(
      'voice search final result submits query and closes dialog', async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('voice-search-final-result', {
          detail: 'test query from speech',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        assertTrue(!!searchbox);
        assertEquals(
            'test query from speech', searchbox.$.input.inputElement.value);

        await testProxy.handler.whenCalled('submitQuery');
        const args = testProxy.handler.getArgs('submitQuery')[0];
        assertEquals('test query from speech', args[0]);
        assertEquals(0, args[1]);  // mouse_button
        assertFalse(args[2]);      // alt_key
        assertFalse(args[3]);      // ctrl_key
        assertFalse(args[4]);      // meta_key
        assertFalse(args[5]);      // shift_key
        assertTrue(args[6]);       // is_voice_search
      });

  test(
      'stopping voice search fills input plate without submitting',
      async () => {
        const searchbox =
            app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
        assertTrue(!!voiceSearch);

        voiceSearch.dispatchEvent(new CustomEvent('recording-stopped', {
          detail: 'stopped speech query',
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
        assertFalse(!!dialog);

        assertEquals(
            'stopped speech query', searchbox.$.input.inputElement.value);
        assertEquals(0, testProxy.handler.getCallCount('submitQuery'));
      });

  test('voice search cancel closes dialog overlay', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
    assertTrue(!!voiceSearch);

    voiceSearch.dispatchEvent(new CustomEvent('voice-search-cancel', {
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    const dialog = app.shadowRoot.querySelector('#voiceSearchDialog');
    assertFalse(!!dialog);
  });

  test('voice permission changed updates CSS class', async () => {
    const searchbox =
        app.shadowRoot.querySelector('omnibox-everywhere-omnibox')!;
    searchbox.dispatchEvent(
        new CustomEvent('open-voice-search', {bubbles: true, composed: true}));
    await microtasksFinished();

    const voiceSearch = app.shadowRoot.querySelector('#voiceSearch')!;
    assertTrue(!!voiceSearch);

    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {isOpened: true},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertTrue(voiceSearch.classList.contains('permission-prompt-showing'));

    voiceSearch.dispatchEvent(new CustomEvent('voice-permission-changed', {
      detail: {isOpened: false},
      bubbles: true,
      composed: true,
    }));
    await microtasksFinished();

    assertFalse(voiceSearch.classList.contains('permission-prompt-showing'));
  });

  test(
      'open-voice-search reflects attribute on app and hides MV tiles and ' +
          'content',
      async () => {
        document.body.innerHTML = window.trustedTypes!.emptyHTML;
        loadTimeData.overrideValues({
          omniboxEverywhereMostVisitedEnabled: true,
        });
        const appWithMv = document.createElement('omnibox-everywhere-app');
        document.body.appendChild(appWithMv);
        await microtasksFinished();

        const searchbox =
            appWithMv.shadowRoot.querySelector('omnibox-everywhere-omnibox');
        const mvContainer = appWithMv.shadowRoot.querySelector<HTMLElement>(
            '#mostVisitedContainer');
        const content =
            appWithMv.shadowRoot.querySelector<HTMLElement>('#content');
        assertTrue(!!searchbox);
        assertTrue(!!mvContainer);
        assertTrue(!!content);
        assertFalse(appWithMv.hasAttribute('show-voice-search-overlay_'));

        searchbox.dispatchEvent(new CustomEvent(
            'open-voice-search', {bubbles: true, composed: true}));
        await microtasksFinished();

        assertTrue(appWithMv.hasAttribute('show-voice-search-overlay_'));

        const dialog = appWithMv.shadowRoot.querySelector<HTMLDialogElement>(
            '#voiceSearchDialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        assertEquals('none', window.getComputedStyle(content).display);
        assertEquals(null, mvContainer.offsetParent);

        const voiceSearch = appWithMv.shadowRoot.querySelector('#voiceSearch');
        assertTrue(!!voiceSearch);
        voiceSearch.dispatchEvent(new CustomEvent('voice-search-cancel', {
          bubbles: true,
          composed: true,
        }));
        await microtasksFinished();

        assertFalse(appWithMv.hasAttribute('show-voice-search-overlay_'));
        assertFalse(!!appWithMv.shadowRoot.querySelector('#voiceSearchDialog'));
        assertEquals('block', window.getComputedStyle(content).display);
        assertEquals('flex', window.getComputedStyle(mvContainer).display);
      });

});

suite('OmniboxEverywhereProfileIconTest', () => {
  let profileIcon: OmniboxEverywhereProfileIconElement;
  let testProxy: TestSearchboxBrowserProxy;

  async function createProfileIcon(profilePickerEnabled: boolean) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    loadTimeData.overrideValues({
      profileAvatarUrl: 'chrome://theme/IDR_PROFILE_AVATAR_0',
      profileName: 'Test Profile',
      profileEmail: 'test@example.com',
      omniboxEverywhereProfilePickerEnabled: profilePickerEnabled,
    });
    testProxy = new TestSearchboxBrowserProxy();
    SearchboxBrowserProxy.setInstance(testProxy);
    profileIcon = document.createElement('omnibox-everywhere-profile-icon');
    document.body.appendChild(profileIcon);
    await microtasksFinished();
  }

  test(
      'profile icon is not clickable or hoverable when profile picker ' +
          'is disabled',
      async () => {
        await createProfileIcon(false);
        const img =
            profileIcon.shadowRoot.querySelector<HTMLElement>('#profileIcon');
        assertTrue(!!img);
        assertFalse(img.classList.contains('clickable'));
        assertEquals('none', window.getComputedStyle(img).pointerEvents);
        assertEquals(
            'none', window.getComputedStyle(profileIcon).pointerEvents);
      });

  test(
      'profile icon is clickable and hoverable when profile picker is enabled',
      async () => {
        await createProfileIcon(true);
        const img =
            profileIcon.shadowRoot.querySelector<HTMLElement>('#profileIcon');
        assertTrue(!!img);
        assertTrue(img.classList.contains('clickable'));
        assertEquals('auto', window.getComputedStyle(img).pointerEvents);
        assertEquals('pointer', window.getComputedStyle(img).cursor);
      });
});
