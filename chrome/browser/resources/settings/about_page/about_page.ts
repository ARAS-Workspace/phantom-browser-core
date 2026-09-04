// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

import '../settings_page/settings_section.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';

import {getCss} from './about_page.css.js';
import {getHtml} from './about_page.html.js';

export interface SettingsAboutPageElement {
  $: {
    productLogo: HTMLImageElement,
    // <if expr="not is_chromeos">
    deprecationWarning: HTMLElement,
    // </if>
  };
}

export class SettingsAboutPageElement extends CrLitElement implements
    SettingsPlugin {
  static get is() {
    return 'settings-about-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      isManaged_: {type: Boolean},
      managedByIcon_: {type: String},

      // <if expr="not is_chromeos">
      obsoleteSystemInfo_: {type: Object},
      // </if>
    };
  }

  protected accessor isManaged_: boolean = loadTimeData.getBoolean('isManaged');
  protected accessor managedByIcon_: string =
      loadTimeData.getString('managedByIcon');

  // <if expr="not is_chromeos">
  protected accessor obsoleteSystemInfo_: {obsolete: boolean} = {
    obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
  };
  // </if>

  protected onManagementPageClick_() {
    window.location.href = loadTimeData.getString('managementPageUrl');
  }

  protected onProductLogoClick_() {
    this.$.productLogo.animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  // SettingsPlugin implementation
  searchContents(query: string) {
    // settings-about-page is intentionally not included in search.
    return Promise.resolve({
      canceled: false,
      matchCount: 0,
      wasClearSearch: query === '',
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-about-page': SettingsAboutPageElement;
  }
}

customElements.define(SettingsAboutPageElement.is, SettingsAboutPageElement);
