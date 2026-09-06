// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {NewTabFooterAppElement} from './app.js';

export function getHtml(this: NewTabFooterAppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<!--
Container for housing the items in the center of the footer that are
separated from each other by a divider.
-->
<div id="container" @contextmenu="${this.onContextmenu_}">
  <div id="spacer"></div>
  <div id="infoContainer">
    ${this.showExtension_ ? html`
      <div id="extensionNameContainer" title="${this.extensionName_}"
          class="notice-item">
        <button @click="${this.onExtensionNameClick_}" role="link"
            aria-label="${this.extensionName_}"
            aria-description="$i18n{manageExtension}">
          ${this.extensionName_}
        </button>
      </div>` : ''}
    ${this.showBackgroundAttribution_ ? html`
      ${this.backgroundAttributionLink_ ?  html`
        <div id="backgroundAttributionContainer" class="notice-item"
            title="${this.backgroundAttributionText_}">
          <button @click="${this.onBackgroundAttributionClick_}" role="link"
              aria-label="${this.backgroundAttributionText_}"
              aria-description="$i18n{backgroundAttributionDesc}">
            ${this.backgroundAttributionText_}
          </button>
        </div>` : html`
        <div id="backgroundAttributionContainer" class="notice-item">
          <p>${this.backgroundAttributionText_}</p>
        </div>`}`
      : ''}
  </div>
  ${!this.showCustomizeButtons_ ? html`<div id="spacer"></div>` : ''}
  ${this.showCustomizeButtons_ ? html`
    <ntp-customize-buttons id="customizeButtons"
        ?info-shown-to-user="${!!this.extensionName_}"
        ?show-customize="${this.isCustomizeActive_}"
        ?show-customize-chrome-text="${this.showCustomizeText_}"
        @customize-click="${this.onCustomizeClick_}">
    </ntp-customize-buttons>` : ''}
</div>
<!--_html_template_end_-->`;
  // clang-format off
}
