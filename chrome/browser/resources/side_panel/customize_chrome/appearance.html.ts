// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppearanceElement} from './appearance.js';

export function getHtml(this: AppearanceElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
${this.showManagedButton_ ? html`
  <customize-chrome-hover-button id="thirdPartyManageLinkButton"
      aria-button-label="${this.i18n('newTabPageManagedByA11yLabel',
                           this.managedByName_)}"
      class="link-out-button theme-button"
      @click="${this.onNewTabPageManageByButtonClick_}"
      label="${this.managedByName_}"
      label-description="${this.managedByDesc_}">
  </customize-chrome-hover-button>
  `: ''}
<customize-color-scheme-mode></customize-color-scheme-mode>
<cr-theme-color-picker id="chromeColors" ?hidden="${!this.showColorPicker_}">
</cr-theme-color-picker>
<hr class="sp-hr" ?hidden="${!this.showClassicChromeButton_}">
<customize-chrome-hover-button id="setClassicChromeButton"
    ?hidden="${!this.showClassicChromeButton_}"
    label="$i18n{resetToClassicChrome}"
    @click="${this.onSetClassicChromeClick_}">
</customize-chrome-hover-button>
${this.showManagedDialog_ ? html`
  <managed-dialog @close="${this.onManagedDialogClose_}"
      title="$i18n{managedColorsTitle}"
      body="$i18n{managedColorsBody}">
  </managed-dialog>
` : ''}
<!--_html_template_end_-->`;
  // clang-format on
}
