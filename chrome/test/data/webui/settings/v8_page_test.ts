// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import type {V8PageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, SiteSettingsBrowserProxyImpl} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
// clang-format on

function createPage(settingsPrefs: SettingsPrefsElement) {
  const page = document.createElement('settings-v8-page');
  page.prefs = settingsPrefs.prefs!;

  document.body.appendChild(page);

  page.setPrefValue('generated.javascript_optimizer', ContentSetting.ALLOW);
  return page;
}

suite('V8Page', function() {
  let page: V8PageElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);
  });

  test('CheckRadioButtons', async function() {
    page = createPage(settingsPrefs);
    await flushTasks();
    assertFalse(!!page.shadowRoot!.querySelector('#blockForUnfamiliarSites'));
    assertTrue(isVisible(page.shadowRoot!.querySelector('#enableForAllSites')));
    assertTrue(isVisible(page.shadowRoot!.querySelector('#blockForAllSites')));
  });
});
