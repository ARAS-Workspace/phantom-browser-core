// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SettingsAboutPageElement} from './about_page.js';

export function getHtml(this: SettingsAboutPageElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<settings-section page-title="$i18n{aboutPageTitle}">
  <div class="cr-row two-line first">
    <img id="productLogo" @click="${this.onProductLogoClick_}"
        srcset="chrome://theme/current-channel-logo@1x 1x,
                chrome://theme/current-channel-logo@2x 2x"
        alt="$i18n{aboutProductLogoAlt}"
        role="presentation">
    <div class="product-title">$i18n{aboutProductTitle}</div>
  </div>
  <div class="cr-row two-line">
    <div class="flex cr-padded-text">
<if expr="not is_chromeos">
      <span id="deprecationWarning"
          ?hidden="${!this.obsoleteSystemInfo_.obsolete}">
        $i18n{aboutObsoleteSystem}
      </span>
</if>
      <div class="secondary">$i18n{aboutBrowserVersion}</div>
    </div>
  </div>
  <cr-link-row class="hr" @click="${this.onManagementPageClick_}"
      start-icon="${this.managedByIcon_}" label="$i18n{managementPage}"
      role-description="$i18n{subpageArrowRoleDescription}"
      ?hidden="${!this.isManaged_}"></cr-link-row>
</settings-section>

<settings-section>
  <div class="info-sections">
    <div class="info-section">
      <div class="secondary">$i18n{aboutProductTitle}</div>
      <div class="secondary">$i18n{aboutProductCopyright}</div>
    </div>
<if expr="_google_chrome or _is_chrome_for_testing_branded">
    <div class="secondary">
      <a id="tos" href="$i18n{aboutTermsURL}">$i18n{aboutProductTos}</a>
    </div>
</if>
  </div>
</settings-section>
<!--_html_template_end_-->`;
  // clang-format on
}
