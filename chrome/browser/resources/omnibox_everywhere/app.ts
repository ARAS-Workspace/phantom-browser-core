// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox.js';
import '/strings.m.js';
import '//resources/cr_components/most_visited/most_visited.js';
import '//resources/cr_components/search/animated_glow.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';

const VOICE_IDLE_TIMEOUT_MS = 8000;
const VOICE_QUERY_LENGTH_LIMIT = 120;

export class OmniboxEverywhereAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-everywhere-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
      searchboxLayoutMode_: {type: String},
      caretAnimationsEnabled_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      showVoiceSearchOverlay_: {
        type: Boolean,
        reflect: true,
      },
      hasVoiceSearchError_: {type: Boolean},
      voiceSearchTranscript_: {type: String},
      voiceSearchReceivedSpeech_: {type: Boolean},
      voiceSearchListening_: {type: Boolean},
      voiceIdleTimeoutMs_: {type: Number},
      voiceQueryLengthLimit_: {type: Number},
      callbackRouter_: {type: Object},
      mostVisitedEnabled_: {type: Boolean},
    };
  }

  protected accessor omniboxPopupDebugEnabled_ =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor showVoiceSearchOverlay_: boolean = false;
  protected accessor hasVoiceSearchError_: boolean = false;
  protected accessor voiceSearchTranscript_: string = '';
  protected accessor voiceSearchReceivedSpeech_: boolean = false;
  protected accessor voiceSearchListening_: boolean = false;
  protected accessor voiceIdleTimeoutMs_: number = VOICE_IDLE_TIMEOUT_MS;
  protected accessor voiceQueryLengthLimit_: number = VOICE_QUERY_LENGTH_LIMIT;
  protected accessor callbackRouter_: PageCallbackRouter =
      SearchboxBrowserProxy.getInstance().callbackRouter;
  protected accessor mostVisitedEnabled_: boolean =
      loadTimeData.getBoolean('omniboxEverywhereMostVisitedEnabled');

  private eventTracker_ = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document.documentElement, 'visibilitychange',
        this.onVisibilitychange_.bind(this));
    this.onVisibilitychange_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  private async onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      return;
    }

    await this.updateComplete;
    const searchbox =
        this.shadowRoot.querySelector('omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.focusInput();
    }
  }

  // TODO(b/540973063): Extract common voice search lifecycle handling into
  // SearchboxMixin.
  protected async onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
    await this.updateComplete;
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && !dialog.open) {
      dialog.showModal();
    }
  }

  protected onVoiceSearchOverlayClose_() {
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && dialog.open) {
      dialog.close();
    }
    this.showVoiceSearchOverlay_ = false;
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = false;
  }

  protected onVoiceSearchCancel_() {
    this.onVoiceSearchOverlayClose_();
  }

  protected onVoiceSearchError_() {
    if (!this.showVoiceSearchOverlay_) {
      return;
    }
    this.hasVoiceSearchError_ = true;
  }

  protected onVoiceSearchRestart_() {
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
  }

  protected onVoiceSearchTranscriptUpdate_(e: CustomEvent<string>) {
    this.voiceSearchTranscript_ = e.detail;
  }

  protected onVoiceSearchSpeechReceived_() {
    this.voiceSearchReceivedSpeech_ = true;
  }

  protected onVoiceSearchDialogClick_(e: MouseEvent) {
    const dialog = e.currentTarget as HTMLDialogElement;
    if (e.target === dialog) {
      this.onVoiceSearchOverlayClose_();
    }
  }

  private handleVoiceSearchResult_(query: string, submit: boolean) {
    this.onVoiceSearchOverlayClose_();
    const trimmedQuery = query?.trim();
    if (!trimmedQuery) {
      return;
    }

    const searchbox =
        this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
            'omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.setInputText(trimmedQuery);
      if (submit) {
        searchbox.pageHandler().submitQuery(
            trimmedQuery, /*mouse_button=*/ 0, /*alt_key=*/ false,
            /*ctrl_key=*/ false, /*meta_key=*/ false, /*shift_key=*/ false,
            /*is_voice_search=*/ true);
        searchbox.clearAutocompleteMatches();
      } else {
        searchbox.focusInput();
        searchbox.queryAutocomplete(trimmedQuery, false, false);
      }
    }
  }

  protected onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    this.handleVoiceSearchResult_(e.detail, /*submit=*/ true);
  }

  protected onVoiceSearchRecordingStopped_(e: CustomEvent<string>) {
    this.handleVoiceSearchResult_(e.detail, /*submit=*/ false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-app': OmniboxEverywhereAppElement;
  }
}

customElements.define(
    OmniboxEverywhereAppElement.is, OmniboxEverywhereAppElement);
