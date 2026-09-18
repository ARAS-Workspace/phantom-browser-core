// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/search/animated_glow.js';
import './profile_icon.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './omnibox.css.js';
import {getHtml} from './omnibox.html.js';
import {UnboundedMenuManager} from './unbounded_utils.js';

export interface OmniboxEverywhereOmniboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

// Note: Copied from omnibox_popup_searchbox.ts.
//       I18nMixinLit may eventually be moved to SearchboxMixin.
const OmniboxEverywhereOmniboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class OmniboxEverywhereOmniboxElement extends
    OmniboxEverywhereOmniboxElementBase implements SearchboxMixinInterface {
  static get is() {
    return 'omnibox-everywhere-omnibox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },
      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },
      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },
      searchboxIcon_: {type: String},
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      animationState: {
        type: String,
        reflect: true,
      },
      inVoiceSearchMode: {
        type: Boolean,
        reflect: true,
      },
      profileAvatarUrl_: {type: String},
      isFuseboxEnabled: {type: Boolean, reflect: true},
      tabSuggestions_: {type: Array},
      searchboxLayoutMode: {type: String},
      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },
      energyEffectAnimationEnabled_: {type: Boolean},
      entrypointName: {type: String},
      screenshotMenuOpen: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor placeholderText: string = '';
  accessor entrypointName: string = 'OmniboxEverywhere';
  accessor isDraggingFile: boolean = false;
  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectAnimationEnabled');
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  protected accessor searchboxIcon_: string =
      '//resources/cr_components/searchbox/icons/google_g.svg';
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor inVoiceSearchMode: boolean = false;
  protected accessor profileAvatarUrl_: string =
      loadTimeData.getString('profileAvatarUrl');
  protected accessor isFuseboxEnabled: boolean =
      loadTimeData.getBoolean('isFuseboxEnabled');
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor searchboxLayoutMode: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor screenshotMenuOpen: boolean = false;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;

  constructor() {
    super();
    const browserProxy = SearchboxBrowserProxy.getInstance();
    this.pageHandler_ = browserProxy.handler;
    this.callbackRouter_ = browserProxy.callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.initialInputScrollHeight = this.$.input.scrollHeight;
  }

  focusInput() {
    this.$.input.focus();
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  //========================================================================
  // SearchboxMixin abstract method implementations
  //========================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  //========================================================================
  // Event handlers
  //========================================================================

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input) {
      return true;
    }
    return !this.$.input.getInputValue().trim();
  }

  protected showVoiceAndLensButtons_(isEnabled: boolean): boolean {
    return isEnabled && this.isInputEmpty();
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    if (this.isFuseboxEnabled) {
      return this.i18n('searchBoxHintAskOrType');
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e);
  }

  protected async onVoiceSearchButtonClick_() {
    this.animationState = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState = GlowAnimationState.LISTENING;
    this.inVoiceSearchMode = true;
    this.dispatchEvent(
        new Event('open-voice-search', {bubbles: true, composed: true}));
  }

  protected onLensSearchClick_(e: Event) {
    this.dropdownIsVisible = false;
    this.screenshotMenuOpen = true;
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#screenshotMenu')!;
    const anchor = e.currentTarget as HTMLElement;
    const rect = anchor.getBoundingClientRect();

    menu.showAtPosition({
      top: rect.top,
      left: rect.left,
      height: rect.height - 2,
      width: rect.width,
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      maxX: Number.MAX_SAFE_INTEGER,
    });

    this.screenshotMenuManager_.onContextMenuOpened();
  }

  protected onScreenshotMenuClose_() {
    this.screenshotMenuOpen = false;
    this.screenshotMenuManager_.onContextMenuClosed();
  }

  protected onScreenshotWindowClick_() {
    // TODO(crbug.com/532197177): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }

  protected onScreenshotEntireScreenClick_() {
    // TODO(crbug.com/532197177): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }

  protected onScreenshotRegionClick_() {
    // TODO(crbug.com/532198850): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
  }

  private unboundedMenuManager_ = new UnboundedMenuManager(
      () => this.shadowRoot?.querySelector('#context') ?? null);

  private screenshotMenuManager_ = new UnboundedMenuManager(
      () => this.shadowRoot?.querySelector('#screenshotMenu') ?? null, () => {
        const menu = this.shadowRoot?.querySelector<CrActionMenuElement>(
            '#screenshotMenu');
        menu?.close();
      });

  protected onContextMenuOpened_() {
    this.unboundedMenuManager_.onContextMenuOpened();
  }

  protected onContextMenuClosed_() {
    this.unboundedMenuManager_.onContextMenuClosed();
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    if (this.unboundedMenuManager_.isDialogOpen() ||
        this.screenshotMenuManager_.isDialogOpen()) {
      return;
    }
    super.onInputWrapperFocusout(e);
  }

}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-omnibox': OmniboxEverywhereOmniboxElement;
  }
}

customElements.define(
    OmniboxEverywhereOmniboxElement.is, OmniboxEverywhereOmniboxElement);
