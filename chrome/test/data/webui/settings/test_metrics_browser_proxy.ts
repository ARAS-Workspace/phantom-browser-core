// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeleteBrowsingDataAction, MetricsBrowserProxy, PrivacyElementInteractions} from 'chrome://settings/settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super([
      'recordAction',
      'recordBooleanHistogram',
      'recordSettingsPageHistogram',
      'recordDeleteBrowsingDataAction',
      // <if expr="_google_chrome and is_win">
      'recordFeatureNotificationsChange',
      // </if>
    ]);
  }

  recordAction(action: string) {
    this.methodCalled('recordAction', action);
  }

  recordBooleanHistogram(histogramName: string, visible: boolean) {
    this.methodCalled('recordBooleanHistogram', [histogramName, visible]);
  }

  recordSettingsPageHistogram(interaction: PrivacyElementInteractions) {
    this.methodCalled('recordSettingsPageHistogram', interaction);
  }

  recordDeleteBrowsingDataAction(action: DeleteBrowsingDataAction) {
    this.methodCalled('recordDeleteBrowsingDataAction', action);
  }

  // <if expr="_google_chrome and is_win">
  recordFeatureNotificationsChange(enabled: boolean) {
    this.methodCalled('recordFeatureNotificationsChange', enabled);
  }
  // </if>
}
