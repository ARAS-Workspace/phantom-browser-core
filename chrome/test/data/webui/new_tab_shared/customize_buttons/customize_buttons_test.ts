// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CustomizeButtonsElement} from 'chrome://newtab-shared/customize_buttons/customize_buttons.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {hasStyle, microtasksFinished} from 'chrome://webui-test/test_util.js';

suite('CustomizeButtons', () => {
  let element: CustomizeButtonsElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    element = document.createElement('ntp-customize-buttons');
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('shows no shadow', () => {
    const button = element.$.customizeButton;
    assertTrue(hasStyle(button, 'box-shadow', 'none'));
  });
});
