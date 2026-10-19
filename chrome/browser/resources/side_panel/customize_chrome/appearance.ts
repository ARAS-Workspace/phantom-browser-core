// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './hover_button.js';
import '/strings.m.js'; // Required by <managed-dialog>.
import 'chrome://resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.js';
import 'chrome://resources/cr_components/theme_color_picker/theme_color_picker.js';
import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './appearance.css.js';
import {getHtml} from './appearance.html.js';
import {CustomizeChromeAction, recordCustomizeChromeAction} from './common.js';
import {NewTabPageType} from './customize_chrome.mojom-webui.js';
import type {Theme} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

export interface AppearanceElement {
  $: {
    chromeColors: HTMLElement,
    setClassicChromeButton: HTMLButtonElement,
  };
}

const AppearanceElementBase = I18nMixinLit(CrLitElement);

export class AppearanceElement extends AppearanceElementBase {
  static get is() {
    return 'customize-chrome-appearance';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      theme_: {type: Object},

      showClassicChromeButton_: {type: Boolean},
      showColorPicker_: {type: Boolean},
      showManagedButton_: {type: Boolean},
      showManagedDialog_: {type: Boolean},
      managedByName_: {type: String},
      managedByDesc_: {type: String},
      newTabPageType_: {type: Number},
      footerEnabled_: {type: Boolean},
    };
  }

  protected accessor theme_: Theme|undefined;
  protected accessor showClassicChromeButton_: boolean = false;
  protected accessor showColorPicker_: boolean = false;
  protected accessor showManagedButton_: boolean = false;
  protected accessor showManagedDialog_: boolean = false;
  private accessor footerEnabled_: boolean =
      loadTimeData.getBoolean('footerEnabled');
  protected accessor newTabPageType_: NewTabPageType = NewTabPageType.kNone;
  protected accessor managedByName_: string = '';
  protected accessor managedByDesc_: string = '';
  private listenerIds_: number[] = [];
  private apiProxy_: CustomizeChromeApiProxy =
      CustomizeChromeApiProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.apiProxy_.callbackRouter.setTheme.addListener(theme => {
        this.theme_ = theme;
      }),
      this.apiProxy_.callbackRouter.attachedTabStateUpdated.addListener(
          newTabPageType => {
            this.newTabPageType_ = newTabPageType;
          }),
      this.apiProxy_.callbackRouter.ntpManagedByNameUpdated.addListener(
          (name, description) => {
            this.managedByName_ = name;
            this.managedByDesc_ = description;
          }),
    ];

    this.apiProxy_.handler.updateTheme();
    this.apiProxy_.handler.updateAttachedTabState();
    this.apiProxy_.handler.updateNtpManagedByName();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    for (const id of this.listenerIds_) {
      assert(this.apiProxy_.callbackRouter.removeListener(id));
    }
    this.listenerIds_ = [];
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('theme_') ||
        changedPrivateProperties.has('newTabPageType_')) {
      this.showClassicChromeButton_ = this.computeShowClassicChromeButton_();
      this.showColorPicker_ = this.computeShowColorPicker_();
    }

    if (changedPrivateProperties.has('managedByName_') ||
        changedPrivateProperties.has('managedByDesc_')) {
      this.showManagedButton_ = this.computeShowManagedButton_();
    }

    // Announce when theme is set to Classic Chrome.
    // This should only be triggered if the classic chrome's button is hidden
    // after the initial theme value has already been set.
    if (changedPrivateProperties.has('theme_') &&
        changedPrivateProperties.has('showClassicChromeButton_') &&
        !!changedPrivateProperties.get('theme_') &&
        !this.showClassicChromeButton_) {
      const announcer = getAnnouncerInstance();
      announcer.announce(this.i18n('updatedToClassicChrome'));
    }
  }

  private computeShowClassicChromeButton_(): boolean {
    if (this.footerEnabled_) {
      return !!(
          this.theme_ && this.theme_.hasBackgroundImage &&
          this.newTabPageType_ === NewTabPageType.kThirdPartyWebUI);
    }
    return !!(
        this.theme_ &&
        (this.theme_.hasBackgroundImage || this.theme_.hasThirdPartyTheme));
  }

  private computeShowColorPicker_(): boolean {
    return !!this.theme_ && !this.theme_.hasThirdPartyTheme;
  }

  private computeShowManagedButton_(): boolean {
    return !!this.managedByName_;
  }

  protected onSetClassicChromeClick_() {
    if (this.handleClickForManagedThemes_()) {
      return;
    }
    this.apiProxy_.handler.removeBackgroundImage();
    this.apiProxy_.handler.setDefaultColor();
    recordCustomizeChromeAction(
        CustomizeChromeAction.SET_CLASSIC_CHROME_THEME_CLICKED);
  }

  protected onManagedDialogClose_() {
    this.showManagedDialog_ = false;
  }

  protected onNewTabPageManageByButtonClick_() {
    this.apiProxy_.handler.openNtpManagedByPage();
  }

  private handleClickForManagedThemes_(): boolean {
    if (!this.theme_ || !this.theme_.backgroundManagedByPolicy) {
      return false;
    }
    this.showManagedDialog_ = true;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-appearance': AppearanceElement;
  }
}

customElements.define(AppearanceElement.is, AppearanceElement);
