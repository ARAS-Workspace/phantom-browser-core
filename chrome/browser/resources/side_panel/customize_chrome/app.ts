// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './appearance.js';
import './customize_toolbar/toolbar.js';
import './footer.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert, assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppearanceElement} from './appearance.js';
import {CustomizeChromeSection, NewTabPageType} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';

const SECTION_TO_SELECTOR = {
  [CustomizeChromeSection.kUnspecified]: '',
  [CustomizeChromeSection.kAppearance]: '#appearance',
  [CustomizeChromeSection.kFooter]: '#footer',
};

export enum CustomizeChromePage {
  OVERVIEW = 'overview',
  TOOLBAR = 'toolbar',
}

export interface AppElement {
  $: {
    overviewPage: HTMLElement,
    appearanceElement: AppearanceElement,
    toolbarButton: HTMLElement,
  };
}

export class AppElement extends CrLitElement {
  static get is() {
    return 'customize-chrome-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      page_: {type: String},
      extensionPolicyEnabled_: {type: Boolean},
      footerEnabled_: {type: Boolean},
      newTabPageType_: {type: Number},
      showFooter_: {type: Boolean},
    };
  }

  protected accessor page_: CustomizeChromePage = CustomizeChromePage.OVERVIEW;
  protected accessor extensionPolicyEnabled_: boolean = false;
  protected accessor footerEnabled_: boolean =
      loadTimeData.getBoolean('footerEnabled');
  protected accessor newTabPageType_: NewTabPageType = NewTabPageType.kNone;
  protected accessor showFooter_: boolean = false;

  private listenerIds_: number[] = [];
  private apiProxy_: CustomizeChromeApiProxy =
      CustomizeChromeApiProxy.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.apiProxy_.callbackRouter.scrollToSection.addListener(
          this.onScrollToSection_.bind(this)),
      this.apiProxy_.callbackRouter.attachedTabStateUpdated.addListener(
          this.onAttachedTabStateUpdated_.bind(this)),
      this.apiProxy_.callbackRouter.setFooterSettings.addListener(
          this.onSetFooterSettings_.bind(this)),
    ];

    this.apiProxy_.handler.updateAttachedTabState();
    this.apiProxy_.handler.updateFooterSettings();

    // We wait for load because `scrollIntoView` above requires the page to be
    // laid out.
    window.addEventListener('load', () => {
      this.apiProxy_.handler.updateScrollToSection();
    }, {once: true});
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
    if (changedPrivateProperties.has('footerEnabled_') ||
        changedPrivateProperties.has('newTabPageType_') ||
        changedPrivateProperties.has('extensionPolicyEnabled_')) {
      this.showFooter_ = this.computeShowFooter_();
    }
  }

  override firstUpdated() {
    ColorChangeUpdater.forDocument().start();
  }

  private onScrollToSection_(section: CustomizeChromeSection) {
    if (section === CustomizeChromeSection.kToolbar) {
      this.openToolbarCustomizationPage();
      chrome.metricsPrivate.recordUserAction(
          'Actions.CustomizeToolbarSidePanel' +
          '.OpenedFromOutsideCustomizeChrome');
      return;
    }

    const selector = SECTION_TO_SELECTOR[section];
    const element = this.shadowRoot.querySelector(selector);
    if (!element) {
      return;
    }

    this.page_ = CustomizeChromePage.OVERVIEW;
    element.scrollIntoView({behavior: 'auto'});
  }

  private onAttachedTabStateUpdated_(newTabPageType: NewTabPageType) {
    if (this.newTabPageType_ === newTabPageType) {
      return;
    }

    this.newTabPageType_ = newTabPageType;
  }

  private onSetFooterSettings_(_: boolean, extensionPolicyEnabled: boolean) {
    this.extensionPolicyEnabled_ = extensionPolicyEnabled;
  }

  protected computeShowFooter_(): boolean {
    return this.footerEnabled_ && this.extensionPolicyEnabled_ &&
        this.newTabPageType_ === NewTabPageType.kExtension;
  }

  protected async onBackClick_() {
    switch (this.page_) {
      case CustomizeChromePage.TOOLBAR:
        this.page_ = CustomizeChromePage.OVERVIEW;
        await this.updateComplete;
        this.$.toolbarButton.focus();
        break;
      case CustomizeChromePage.OVERVIEW:
        assertNotReached();
      default:
        assertNotReachedCase(this.page_);
    }
  }

  protected onToolbarCustomizationButtonClick_() {
    this.openToolbarCustomizationPage();
    chrome.metricsPrivate.recordUserAction(
        'Actions.CustomizeToolbarSidePanel.OpenedFromCustomizeChrome');
  }

  private async openToolbarCustomizationPage() {
    this.page_ = CustomizeChromePage.TOOLBAR;
    const page = this.shadowRoot.querySelector('customize-chrome-toolbar');
    assert(page);
    await this.updateComplete;
    page.focusOnBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-chrome-app': AppElement;
  }
}

customElements.define(AppElement.is, AppElement);
