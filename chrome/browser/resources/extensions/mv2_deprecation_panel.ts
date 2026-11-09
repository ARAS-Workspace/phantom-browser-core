// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from './item.js';
import type {Mv2DeprecationDelegate} from './mv2_deprecation_delegate.js';
import {getCss} from './mv2_deprecation_panel.css.js';
import {getHtml} from './mv2_deprecation_panel.html.js';

const ExtensionsMv2DeprecationPanelElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsMv2DeprecationPanelElement extends
    ExtensionsMv2DeprecationPanelElementBase {
  static get is() {
    return 'extensions-mv2-deprecation-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},

      /*
       * Extensions to display in the panel.
       */
      extensions: {type: Array},

      /**
       * Whether the panel title should be shown.
       */
      showTitle: {
        type: Boolean,
        reflect: true,
      },

      /**
       * The string for the panel's header.
       */
      headerString_: {type: String},

      webuiRoundedIconsEnabled_: {type: Boolean},
    };
  }

  accessor extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  accessor delegate: ItemDelegate&Mv2DeprecationDelegate|undefined;
  accessor showTitle: boolean = false;
  protected accessor headerString_: string = '';
  protected accessor webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('extensions')) {
      this.onExtensionsChanged_();
    }
  }

  /**
   * Updates properties after extensions change.
   */
  private async onExtensionsChanged_(): Promise<void> {
    const headerVar = 'mv2DeprecationPanelDisabledHeader';

    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            headerVar, this.extensions.length);
  }

  /**
   * Returns whether the extension's remove button should be displayed.
   */
  protected showExtensionRemoveButton_(
      extension: chrome.developerPrivate.ExtensionInfo): boolean {
    return !extension.mustRemainInstalled;
  }

  /**
   * Returns the accessible label for the remove button corresponding to
   * `extensionName`.
   */
  protected getRemoveButtonLabelFor_(extensionName: string): string {
    return this.i18n('mv2DeprecationPanelRemoveButtonAccLabel', extensionName);
  }

  /**
   * Triggers the MV2 deprecation notice dismissal when the dismiss button is
   * clicked.
   */
  protected onDismissButtonClick_() {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Unsupported.Dismissed');
    assert(this.delegate);
    this.delegate.dismissMv2DeprecationNotice();
  }

  /**
   * Triggers an extension removal when the remove button is clicked for an
   * extension.
   */
  protected onRemoveButtonClick_(event: Event): void {
    chrome.metricsPrivate.recordUserAction(
        'Extensions.Mv2Deprecation.Unsupported.RemoveExtension');

    const id = (event.target as HTMLElement).dataset['id'];
    assert(!!id);
    assert(this.delegate);
    this.delegate.deleteItem(id);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-mv2-deprecation-panel': ExtensionsMv2DeprecationPanelElement;
  }
}

customElements.define(
    ExtensionsMv2DeprecationPanelElement.is,
    ExtensionsMv2DeprecationPanelElement);
