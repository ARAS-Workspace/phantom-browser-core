// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://newtab-footer/shared/customize_buttons/customize_buttons.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {BrowserProxy as CustomizeButtonsBrowserProxy} from './customize_buttons.mojom-webui.js';
import {browserProxyFactory as customizeButtonsProxyFactory, SidePanelOpenTrigger} from './customize_buttons.mojom-webui.js';
import {CustomizeChromeSection} from './customize_chrome.mojom-webui.js';
import type {BrowserProxy as NewTabFooterBrowserProxy} from './new_tab_footer.mojom-webui.js';
import {browserProxyFactory as newTabFooterProxyFactory, NewTabPageType} from './new_tab_footer.mojom-webui.js';

/**
 * Customize Chrome entry points. This enum must match the numbering for
 * FooterCustomizeChromeEntryPoint in enums.xml. These values are persisted to
 * logs. Entries should not be renumbered, removed or reused.
 */
export enum FooterCustomizeChromeEntryPoint {
  CUSTOMIZE_BUTTON = 0,
  URL = 1,
  MAX_VALUE = URL,
}

/**
 * Elements on the New Tab Footer. This enum must match the numbering for
 * FooterElement in enums.xml. These values are persisted to logs. Entries
 * should not be renumbered, removed or reused.
 */
export enum FooterElement {
  OTHER = 0,
  CUSTOMIZE_BUTTON = 1,
  EXTENSION_NAME = 2,
  MANAGEMENT_NOTICE = 3,
  BACKGROUND_ATTRIBUTION = 4,
  CONTEXT_MENU = 5,
  MAX_VALUE = CONTEXT_MENU,
}

function recordCustomizeChromeOpen(element: FooterCustomizeChromeEntryPoint) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.CustomizeChromeOpened', element,
      FooterCustomizeChromeEntryPoint.MAX_VALUE + 1);
}

function recordClick(element: FooterElement) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Footer.Click', element, FooterElement.MAX_VALUE + 1);
}

export class NewTabFooterAppElement extends CrLitElement {
  static get is() {
    return 'new-tab-footer-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      extensionName_: {type: String},
      isCustomizeActive_: {type: Boolean},
      showCustomizeButtons_: {type: Boolean},
      showCustomizeText_: {type: Boolean},
      showExtension_: {type: Boolean},
      ntpType_: {type: Number},
    };
  }

  protected accessor extensionName_: string|null = null;
  protected accessor isCustomizeActive_: boolean = false;
  protected accessor showCustomizeButtons_: boolean = false;
  protected accessor showCustomizeText_: boolean = true;
  protected accessor showExtension_: boolean = false;
  protected accessor ntpType_: NewTabPageType = NewTabPageType.kOther;

  private canCustomizeChrome_: boolean = false;
  private browserProxy_: NewTabFooterBrowserProxy;
  private customizeButtonsBrowserProxy_: CustomizeButtonsBrowserProxy;
  private setCustomizeChromeSidePanelVisibilityListener_: number|null = null;
  private setNtpExtensionNameListenerId_: number|null = null;
  private setAttachedTabStateUpdatedListener_: number|null = null;

  constructor() {
    super();
    this.browserProxy_ = newTabFooterProxyFactory.getInstance();
    this.customizeButtonsBrowserProxy_ =
        customizeButtonsProxyFactory.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setNtpExtensionNameListenerId_ =
        this.browserProxy_.callbackRouter.setNtpExtensionName.addListener(
            name => {
              this.extensionName_ = name;
            });
    this.browserProxy_.handler.updateNtpExtensionName();
    this.setCustomizeChromeSidePanelVisibilityListener_ =
        this.customizeButtonsBrowserProxy_.callbackRouter
            .setCustomizeChromeSidePanelVisibility.addListener(visible => {
              this.isCustomizeActive_ = visible;
            });
    this.setAttachedTabStateUpdatedListener_ =
        this.browserProxy_.callbackRouter.attachedTabStateUpdated.addListener(
            (ntpType, canCustomizeChrome) => {
              this.ntpType_ = ntpType;
              this.canCustomizeChrome_ = canCustomizeChrome;
            });
    this.browserProxy_.handler.updateAttachedTabState();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.setNtpExtensionNameListenerId_);
    this.browserProxy_.callbackRouter.removeListener(
        this.setNtpExtensionNameListenerId_);
    assert(this.setAttachedTabStateUpdatedListener_);
    this.browserProxy_.callbackRouter.removeListener(
        this.setAttachedTabStateUpdatedListener_);
    assert(this.setCustomizeChromeSidePanelVisibilityListener_);
    this.customizeButtonsBrowserProxy_.callbackRouter.removeListener(
        this.setCustomizeChromeSidePanelVisibilityListener_);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('ntpType_')) {
      this.showCustomizeButtons_ = this.computeShowCustomizeButtons_();
    }

    if (changedPrivateProperties.has('ntpType_') ||
        changedPrivateProperties.has('extensionName_')) {
      this.showExtension_ = this.computeShowExtension_();
    }
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  private computeShowCustomizeButtons_(): boolean {
    return this.canCustomizeChrome_ &&
        this.ntpType_ === NewTabPageType.kExtension;
  }

  private computeShowExtension_(): boolean {
    return !!this.extensionName_ && this.ntpType_ === NewTabPageType.kExtension;
  }

  protected onContextmenu_(e: MouseEvent) {
    this.browserProxy_.handler.showContextMenu({x: e.clientX, y: e.clientY});
    recordClick(FooterElement.CONTEXT_MENU);
  }

  protected onExtensionNameClick_(e: Event) {
    e.preventDefault();
    recordClick(FooterElement.EXTENSION_NAME);
    this.browserProxy_.handler.openExtensionOptionsPageWithFallback();
  }

  protected onCustomizeClick_() {
    recordClick(FooterElement.CUSTOMIZE_BUTTON);
    this.setCustomizeChromeSidePanelVisible_(!this.isCustomizeActive_);
    if (!this.isCustomizeActive_) {
      recordCustomizeChromeOpen(
          FooterCustomizeChromeEntryPoint.CUSTOMIZE_BUTTON);
    }
  }

  private setCustomizeChromeSidePanelVisible_(visible: boolean) {
    this.customizeButtonsBrowserProxy_.handler
        .setCustomizeChromeSidePanelVisible(
            visible, CustomizeChromeSection.kUnspecified,
            SidePanelOpenTrigger.kNewTabFooter);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'new-tab-footer-app': NewTabFooterAppElement;
  }
}

customElements.define(NewTabFooterAppElement.is, NewTabFooterAppElement);
