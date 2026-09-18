// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {PlaceholderTextCycler} from '//resources/cr_components/searchbox/placeholder_text_cycler.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {waitForLazyRender} from '//resources/cr_components/searchbox/utils.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './ntp_searchbox.css.js';
import {getHtml} from './ntp_searchbox.html.js';

export interface NtpSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const NtpSearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

/** A search box for the NTP that behaves like the Omnibox. */
export class NtpSearchboxElement extends NtpSearchboxElementBase implements
    SearchboxMixinInterface {
  static get is() {
    return 'ntp-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================
      ntpRealboxNextEnabled: {
        type: Boolean,
        reflect: true,
      },

      cyclingPlaceholders: {type: Boolean},

      isDraggingFile: {
        reflect: true,
        type: Boolean,
      },

      animationState: {
        reflect: true,
        type: String,
      },

      colorSourceIsBaseline: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the theme is dark. */
      isDark: {
        type: Boolean,
        reflect: true,
      },

      searchboxLayoutMode: {
        type: String,
        reflect: true,
      },

      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      inVoiceSearchMode: {
        type: Boolean,
        reflect: true,
      },

      /**
       * Whether the secondary side was at any point available to be shown.
       */
      hadSecondarySide: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },

      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      hasVoiceSearchError: {type: Boolean},

      isListening: {type: Boolean},

      //========================================================================
      // Protected properties
      //========================================================================
      tabSuggestions_: {type: Array},
      recentTabId_: {type: Number},

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      /** Whether the voice search icon should be visible in the searchbox. */
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      /** Whether the Google Lens icon should be visible in the searchbox. */
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },

      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      energyEffectAnimationEnabled: {type: Boolean},
      keepMenuOpenOnTabSelectForRealbox: {type: Boolean},
      smartTabSharingActive: {type: Boolean},
    };
  }

  accessor ntpRealboxNextEnabled: boolean = false;
  accessor smartTabSharingActive: boolean = false;
  accessor energyEffectAnimationEnabled: boolean = false;
  accessor cyclingPlaceholders: boolean = false;
  accessor isDraggingFile: boolean = false;
  accessor animationState: GlowAnimationState = GlowAnimationState.NONE;
  accessor colorSourceIsBaseline: boolean = false;
  accessor isDark: boolean = false;
  accessor searchboxLayoutMode: string = '';
  accessor canShowSecondarySide: boolean = false;
  accessor hadSecondarySide: boolean = false;
  accessor hasSecondarySide: boolean = false;
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  accessor keepMenuOpenOnTabSelectForRealbox: boolean =
      loadTimeData.getBoolean('keepMenuOpenOnTabSelectForRealbox');
  accessor placeholderText: string = '';
  accessor recentTabId_: number|null = null;

  accessor inVoiceSearchMode: boolean = false;
  // If voice search error scrim is showing:
  accessor hasVoiceSearchError: boolean = false;
  // Voice search is listening if there is no error and voice search overlay
  // is open (and active).
  accessor isListening: boolean = false;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = false;
  protected callbackRouter_: PageCallbackRouter;

  private placeholderCycler_: PlaceholderTextCycler|null = null;
  private onTabStripChangedListenerId_: number|null = null;
  private pageHandler_: PageHandlerInterface;
  private autocompleteResultChangedListenerId_: number|null = null;
  private inputStateListenerId_: number|null = null;

  constructor() {
    performance.mark('searchbox-creation-start');
    super();

    this.pageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
  }

  override async connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));

    // <if expr="not is_android">
    this.smartTabSharingActive =
        (await this.pageHandler().getSmartTabSharingActive()).active;
    // </if>
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }

    if (this.inputStateListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.inputStateListenerId_);
      this.inputStateListenerId_ = null;
    }

    this.placeholderCycler_?.stop();
    if (this.onTabStripChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.onTabStripChangedListenerId_);
      this.onTabStripChangedListenerId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming') ||
        changedProperties.has('colorSourceIsBaseline')) {
      this.useWebkitSearchIcons_ =
          this.searchboxChromeRefreshTheming && !this.colorSourceIsBaseline;
    }

    if (changedProperties.has('inVoiceSearchMode') ||
        changedProperties.has('hasVoiceSearchError')) {
      this.isListening = this.inVoiceSearchMode && !this.hasVoiceSearchError;
    }
  }

  override firstUpdated() {
    // After crbug.com/502367598, there is no super.firstUpdated() call, the new
    // base chain does not override firstUpdated().
    if (performance.getEntriesByName('realbox-creation-start').length > 0) {
      performance.measure('realbox-creation', 'realbox-creation-start');
    }
    this.initialInputScrollHeight = this.$.input.scrollHeight;

    if (this.cyclingPlaceholders) {
      waitForLazyRender().then(async () => {
        const {config} = await this.pageHandler().getCyclingPlaceholderConfig();
        const texts = config.texts;
        if (texts.length < 2) {
          // Need at least 2 placeholders to cycle. If fewer, disable cycling
          // and let the static placeholder text show instead.
          return;
        }
        this.placeholderText = texts[0]!;
        this.placeholderCycler_ = new PlaceholderTextCycler(
            this.$.input.inputElement, texts,
            Number(config.changeTextAnimationInterval.microseconds / 1000n),
            Number(config.fadeTextAnimationDuration.microseconds / 1000n));
        this.placeholderCycler_.start();
      });
    }
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    return isEnabled && this.isInputEmpty();
  }

  override handleKeyNavigation(e: KeyboardEvent) {

    if (!this.dropdownIsVisible &&
        (e.key === 'ArrowUp' || e.key === 'ArrowDown') &&
        this.multiLineEnabled &&
        this.shouldSuppressDropdownForMultiline_(
            this.result?.matches?.length || 0)) {
      return;
    }

    super.handleKeyNavigation(e);
  }

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
    this.placeholderCycler_?.stop();
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    super.onInputWrapperFocusout(e);
    this.placeholderCycler_?.start();
  }

  //============================================================================
  // Mixin abstract method implementations
  //============================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  override updateDropdownVisibility(): void {
    if (!this.result) {
      this.dropdownIsVisible = false;
      return;
    }

    const hasPrimaryMatches = this.result.matches?.some(match => {
      const sideType =
          this.result!.suggestionGroupsMap[match.suggestionGroupId]?.sideType ||
          SideType.kDefaultPrimary;
      return sideType === SideType.kDefaultPrimary;
    });

    this.dropdownIsVisible = hasPrimaryMatches;

    // In multi-line mode, suppress the dropdown when text wraps or when the
    // only match is the mirror query.
    if (this.multiLineEnabled && this.dropdownIsVisible) {
      const isUserTyping = this.result.input.trim().length > 0;
      if (isUserTyping &&
          this.shouldSuppressDropdownForMultiline_(
              this.result.matches?.length || 0)) {
        this.dropdownIsVisible = false;
      }
    }
  }

  //============================================================================
  // Public API (ported from SearchboxElement)
  //============================================================================

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  focusInput() {
    this.$.input.focus();
  }

  blurInput() {
    this.$.input.blur();
  }

  selectAll() {
    this.$.input.select();
  }

  //============================================================================
  // Event handlers
  //============================================================================

  // Perform animation work in this file (`ntp_searchbox.ts`) since this
  // file owns the animation state while `new_tab_page/app.ts` owns the
  // voice component and its initialization.
  async onVoiceSearchClick() {
    this.animationState = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState = GlowAnimationState.LISTENING;
    this.inVoiceSearchMode = true;
    // `new_tab_page/app.ts` controls the voice component and when it will
    // start. `new_tab_page/app.ts` sets `inVoiceSearchMode` to `true`
    // with `new_tab_page/app.ts`'s equivalent state
    // `showVoiceSearchOverlay`.
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onSmartTabSharingActiveChanged_(
      _e: CustomEvent<{active: boolean}>) {
    // <if expr="not is_android">
    this.smartTabSharingActive = _e.detail.active;
    this.pageHandler().setSmartTabSharingActive(_e.detail.active);
    // </if>
  }

  protected onContextMenuClosed_() {
    this.blur();
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
    this.dispatchEvent(new Event('context-menu-entrypoint-click'));
  }

  protected useCompactLayout_(): boolean {
    return this.searchboxLayoutMode === 'Compact';
  }

  protected onSearchboxInputPasted_() {
    chrome.histograms.recordCount('NewTabPage.Realbox.Paste', 1);
    chrome.histograms.recordUserAction('NewTabPage.Realbox.Paste');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e);
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }

  protected onHadSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hadSecondarySide = e.detail.value;
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }

  //============================================================================
  // Helpers
  //============================================================================

  protected inputHasMatches_(): boolean {
    return !!this.result && !!this.result.matches &&
        this.result.matches.length > 0;
  }

  private shouldSuppressDropdownForMultiline_(numMatches: number): boolean {
    const inputHasWrapped = this.initialInputScrollHeight > 0 &&
        this.$.input.scrollHeight > this.initialInputScrollHeight;
    return inputHasWrapped || numMatches === 1;
  }

  protected computePlaceholderText_(placeholderText: string): string {
    return placeholderText || '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-searchbox': NtpSearchboxElement;
  }
}

customElements.define(NtpSearchboxElement.is, NtpSearchboxElement);
