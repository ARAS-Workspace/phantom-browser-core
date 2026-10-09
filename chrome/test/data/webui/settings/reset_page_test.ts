// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {SettingsResetPageElement, SettingsResetProfileDialogElement} from 'chrome://settings/lazy_load.js';
import {ResetBrowserProxyImpl, routes} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestResetBrowserProxy} from './test_reset_browser_proxy.js';

// clang-format on

const TestNames = {
  ResetProfileDialogAction: 'ResetProfileDialogAction',
  ResetProfileDialogOpenClose: 'ResetProfileDialogOpenClose',
};

suite('DialogTests', function() {
  let resetPage: SettingsResetPageElement;
  let resetPageBrowserProxy: TestResetBrowserProxy;

  setup(function() {
    resetPageBrowserProxy = new TestResetBrowserProxy();
    ResetBrowserProxyImpl.setInstance(resetPageBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    resetPage = document.createElement('settings-reset-page');
    document.body.appendChild(resetPage);
  });

  teardown(function() {
    resetPage.remove();
  });

  /**
   * @param closeDialogFn A function to call for closing the dialog.
   */
  async function testOpenCloseResetProfileDialog(
      closeDialogFn: (dialog: SettingsResetProfileDialogElement) => void) {

    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    await microtasksFinished();
    const dialog =
        resetPage.shadowRoot.querySelector('settings-reset-profile-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);

    const whenDialogClosed = eventToPromise('close', dialog);

    closeDialogFn(dialog);
    await whenDialogClosed;
  }

  // Tests that the reset profile dialog opens and closes correctly and that
  // resetPageBrowserProxy calls are occurring as expected.
  test(TestNames.ResetProfileDialogOpenClose, async function() {
    // Test case where the 'cancel' button is clicked.
    await testOpenCloseResetProfileDialog(dialog => {
      dialog.$.cancel.click();
    });
    // Test case where the browser's 'back' button is clicked.
    await testOpenCloseResetProfileDialog(_dialog => {
      resetPage.currentRouteChanged(routes.BASIC);
    });
  });

  // Tests that when user request to reset the profile the appropriate
  // message is sent to the browser.
  test(TestNames.ResetProfileDialogAction, async function() {
    resetPageBrowserProxy.setPerformResetProfileSettingsPromise();
    // Open reset profile dialog.
    resetPage.$.resetProfile.click();
    await microtasksFinished();
    const dialog =
        resetPage.shadowRoot.querySelector('settings-reset-profile-dialog');
    assertTrue(!!dialog);

    assertFalse(dialog.$.reset.disabled);
    const spinner = dialog.shadowRoot.querySelector('.spinner');
    assertTrue(!!spinner);
    assertFalse(isVisible(spinner));
    dialog.$.reset.click();
    await microtasksFinished();
    assertTrue(dialog.$.reset.disabled);
    assertTrue(dialog.$.cancel.disabled);
    assertTrue(isVisible(spinner));

    resetPageBrowserProxy.resolvePerformResetProfileSettings();
    await resetPageBrowserProxy.whenCalled('performResetProfileSettings');
  });

  test('searchContents', async function() {
    let result = await resetPage.searchContents('restore');
    assertFalse(result.canceled);
    assertEquals(1, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await resetPage.searchContents('non-existing-text');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertFalse(result.wasClearSearch);

    result = await resetPage.searchContents('');
    assertFalse(result.canceled);
    assertEquals(0, result.matchCount);
    assertTrue(result.wasClearSearch);
  });
});
