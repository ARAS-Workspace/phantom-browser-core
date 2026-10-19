// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://customize-chrome-side-panel.top-chrome/shared/sp_heading.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './appearance.js';
import './categories.js';
import './customize_toolbar/toolbar.js';
import './footer.js';
import './themes.js';
import './wallpaper_search/wallpaper_search.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert, assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {AppearanceElement} from './appearance.js';
import type {CategoriesElement} from './categories.js';
import type {BackgroundCollection} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeSection, NewTabPageType} from './customize_chrome.mojom-webui.js';
import {CustomizeChromeApiProxy} from './customize_chrome_api_proxy.js';
import type {ThemesElement} from './themes.js';

const SECTION_TO_SELECTOR = {
  [CustomizeChromeSection.kUnspecified]: '',
  [CustomizeChromeSection.kAppearance]: '#appearance',
  [CustomizeChromeSection.kFooter]: '#footer',
};

export enum CustomizeChromePage {
  OVERVIEW = 'overview',
  CATEGORIES = 'categories',
  THEMES = 'themes',
  TOOLBAR = 'toolbar',
  WALLPAPER_SEARCH = 'wallpaper-search',
}

export interface AppElement {
  $: {
    overviewPage: HTMLElement,
    categoriesPage: CategoriesElement,
    themesPage: ThemesElement,
    appearanceElement: AppearanceElement,
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
      selectedCollection_: {type: Object},
      extensionPolicyEnabled_: {type: Boolean},
      footerEnabled_: {type: Boolean},
      wallpaperSearchEnabled_: {type: Boolean},
      newTabPageType_: {type: Number},
      showEditTheme_: {type: Boolean},
      showFooter_: {type: Boolean},
    };
  }

  protected accessor page_: CustomizeChromePage = CustomizeChromePage.OVERVIEW;
  protected accessor selectedCollection_: BackgroundCollection|null = null;
  protected accessor extensionPolicyEnabled_: boolean = false;
  protected accessor footerEnabled_: boolean =
      loadTimeData.getBoolean('footerEnabled');
  protected accessor wallpaperSearchEnabled_: boolean =
      loadTimeData.getBoolean('wallpaperSearchEnabled');
  protected accessor newTabPageType_: NewTabPageType = NewTabPageType.kNone;
  protected accessor showEditTheme_: boolean = false;
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
      this.apiProxy_.callbackRouter.setThemeEditable.addListener(
          this.onSetThemeEditable_.bind(this)),
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

    // Only the toolbar page does not depend on the attached tab, so change any
    // other page back to the overview.
    if (this.page_ !== CustomizeChromePage.TOOLBAR) {
      this.page_ = CustomizeChromePage.OVERVIEW;
    }
  }

  private onSetThemeEditable_(isThemeEditable: boolean) {
    this.showEditTheme_ = isThemeEditable;
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
      case CustomizeChromePage.CATEGORIES:
      case CustomizeChromePage.TOOLBAR:
        this.page_ = CustomizeChromePage.OVERVIEW;
        await this.updateComplete;
        this.$.appearanceElement.focusOnThemeButton();
        break;
      case CustomizeChromePage.THEMES:
      case CustomizeChromePage.WALLPAPER_SEARCH:
        this.page_ = CustomizeChromePage.CATEGORIES;
        await this.updateComplete;
        this.$.categoriesPage.focusOnBackButton();
        break;
      case CustomizeChromePage.OVERVIEW:
        assertNotReached();
      default:
        assertNotReachedCase(this.page_);
    }
  }

  protected async onEditThemeClick_() {
    this.page_ = CustomizeChromePage.CATEGORIES;
    await this.updateComplete;
    this.$.categoriesPage.focusOnBackButton();
  }

  protected async onCollectionSelect_(
      event: CustomEvent<BackgroundCollection>) {
    this.selectedCollection_ = event.detail;
    this.page_ = CustomizeChromePage.THEMES;
    await this.updateComplete;
    this.$.themesPage.focusOnBackButton();
  }

  protected async onLocalImageUpload_() {
    this.page_ = CustomizeChromePage.OVERVIEW;
    await this.updateComplete;
    this.$.appearanceElement.focusOnThemeButton();
  }

  protected onWallpaperSearchClick_() {
    this.onWallpaperSearchSelect_();
  }

  protected onWallpaperSearchSelect_() {
    this.page_ = CustomizeChromePage.WALLPAPER_SEARCH;
    const page =
        this.shadowRoot.querySelector('customize-chrome-wallpaper-search');
    assert(page);
    page.focusOnBackButton();
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
