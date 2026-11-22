// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings help page. */

import 'chrome://settings/settings.js';

import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('SettingsHelpPage', function() {
  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // The ChromeContentBrowserClient will rewrite chrome://help to
    // chrome://settings/help.
    window.history.pushState('', 'Test', 'chrome://settings/help');
  });

  test('about section', async () => {
    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
    const settingsMain = settingsUi.shadowRoot!.querySelector('settings-main');
    assertTrue(!!settingsMain);
    await flushTasks();
    const aboutPage =
        settingsMain.shadowRoot!.querySelector('settings-about-page');
    assertTrue(!!aboutPage);
  });

  test('credits Chromium above Phantom Browser', async () => {
    const settingsUi = document.createElement('settings-ui');
    document.body.appendChild(settingsUi);
    const settingsMain = settingsUi.shadowRoot!.querySelector('settings-main');
    assertTrue(!!settingsMain);
    await flushTasks();
    const aboutPage =
        settingsMain.shadowRoot!.querySelector('settings-about-page');
    assertTrue(!!aboutPage);
    const lines = Array.from(
        aboutPage.shadowRoot.querySelectorAll('.info-section'),
        section => Array.from(
            section.querySelectorAll('.secondary'),
            line => line.textContent.trim()));
    assertEquals(2, lines.length);
    assertEquals('Chromium', lines[0]![0]);
    assertTrue(
        /^Copyright \d{4} The Chromium Authors\. All rights reserved\.$/.test(
            lines[0]![1]!));
    assertEquals('Phantom Browser', lines[1]![0]);
    assertTrue(/^Copyright \d{4} R\. Emre ARAS\. All rights reserved\.$/.test(
        lines[1]![1]!));
  });
});
