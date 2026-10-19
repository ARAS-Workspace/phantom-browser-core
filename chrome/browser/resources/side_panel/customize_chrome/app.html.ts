// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {AppElement} from './app.js';

export function getHtml(this: AppElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<cr-page-selector id="container"
    class="sp-scroller sp-scroller-top-of-page sp-scroller-bottom-of-page"
    selected="${this.page_}" attr-for-selected="page-name">
  <div page-name="overview" id="overviewPage">
    <div id="appearance" class="section sp-card">
      <sp-heading hide-back-button>
        <h2 slot="heading">$i18n{appearanceHeader}</h2>
      </sp-heading>
      <customize-chrome-appearance id="appearanceElement">
      </customize-chrome-appearance>
    </div>
    <hr class="sp-cards-separator">
    <cr-button id="toolbarButton" class="section sp-card"
        @click="${this.onToolbarCustomizationButtonClick_}">
      <sp-heading hide-back-button id="toolbar-customization-heading">
        <h2 slot="heading" id="toolbar-customization-inner-heading"
            aria-label="$i18n{toolbarButtonA11yLabel}">
          $i18n{toolbarHeader}
        </h2>
      </sp-heading>
      <cr-icon icon="cr:chevron-right" slot="suffix-icon"></cr-icon>
    </cr-button>
    ${this.showFooter_ ? html`
      <hr class="sp-cards-separator">
      <div id="footer" class="section sp-card">
        <sp-heading hide-back-button>
          <h2 slot="heading">$i18n{footerHeader}</h2>
        </sp-heading>
        <customize-chrome-footer></customize-chrome-footer>
      </div>
    `: ''}
  </div>
  <customize-chrome-toolbar @back-click="${this.onBackClick_}"
      page-name="toolbar" id="toolbarPage"></customize-chrome-toolbar>
</cr-page-selector>
<!--_html_template_end_-->`;
  // clang-format on
}
