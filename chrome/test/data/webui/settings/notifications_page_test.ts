// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {NotificationsPageElement} from 'chrome://settings/lazy_load.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingsBrowserProxyImpl, SettingsState} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestSiteSettingsBrowserProxy} from './test_site_settings_browser_proxy.js';
import type {SiteSettingsPref} from './test_util.js';
import {createContentSettingTypeToValuePair, createDefaultContentSetting, createSiteSettingsPrefs} from './test_util.js';

// clang-format on

function createPref(
    category: ContentSettingsTypes,
    contentSetting: ContentSetting): SiteSettingsPref {
  return createSiteSettingsPrefs(
      [
        createContentSettingTypeToValuePair(
            category, createDefaultContentSetting({
              setting: contentSetting,
            })),
      ],
      []);
}

suite(`NotificationsPage`, function() {
  let page: NotificationsPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let siteSettingsBrowserProxy: TestSiteSettingsBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    page = document.createElement('settings-notifications-page');
    page.prefs = settingsPrefs.prefs!;
    document.body.appendChild(page);
    return flushTasks();
  }

  setup(function() {
    siteSettingsBrowserProxy = new TestSiteSettingsBrowserProxy();
    SiteSettingsBrowserProxyImpl.setInstance(siteSettingsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    return createPage();
  });

  teardown(function() {
    page.remove();
  });

  test('NotificationPage', function() {
    const notificationRadioGroup =
        page.shadowRoot!.querySelector('#notificationRadioGroup');
    assertTrue(!!notificationRadioGroup);

    const categorySettingExceptions =
        page.shadowRoot!.querySelector('category-setting-exceptions');
    assertTrue(!!categorySettingExceptions);
    assertTrue(isVisible(categorySettingExceptions));
    assertEquals(
        ContentSettingsTypes.NOTIFICATIONS, categorySettingExceptions.category);
  });

  test('notificationCPSS', async function() {
    siteSettingsBrowserProxy.setPrefs(
        createPref(ContentSettingsTypes.NOTIFICATIONS, ContentSetting.ASK));

    const cpssRadioGroup =
        page.shadowRoot!.querySelector('settings-radio-group');
    assertTrue(!!cpssRadioGroup);

    const radioGroup = page.shadowRoot!.querySelector<HTMLElement>(
        'settings-category-default-radio-group');
    assertTrue(!!radioGroup);
    assertTrue(isVisible(radioGroup));
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.notification.value'));

    const blockNotification =
        radioGroup.shadowRoot!.querySelector<HTMLElement>('#blockRadioOption');
    assertTrue(!!blockNotification);
    blockNotification.click();
    await flushTasks();
    assertFalse(isVisible(cpssRadioGroup));

    const askForNotification =
        radioGroup.shadowRoot!.querySelector<HTMLElement>('#askRadioOption');
    assertTrue(!!askForNotification);
    askForNotification.click();
    await flushTasks();
    assertTrue(isVisible(cpssRadioGroup));
    assertEquals(
        SettingsState.CPSS, page.get('prefs.generated.notification.value'));
  });
});
