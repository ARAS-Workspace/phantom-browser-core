// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';

export function getHtml(this: OmniboxEverywhereOmniboxElement) {
  return html`
    <div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
        @keydown="${this.onInputWrapperKeydown}">
      <search-animated-glow
        .animationState="${this.animationState}"
        .energyEffectAnimationEnabled="${this.energyEffectAnimationEnabled_}"
        .entrypointName="${this.entrypointName}"
        part="animated-glow">
      </search-animated-glow>
      <cr-searchbox-input id="input"
          exportparts="searchbox-input"
          ?dropdown-is-visible="${this.dropdownIsVisible}"
          input-aria-live="${this.inputAriaLive}"
          ?multi-line-enabled="${this.multiLineEnabled}"
          placeholder-text="${this.computePlaceholderText_()}"
          searchbox-aria-description="${this.searchboxAriaDescription}"
          searchbox-icon="${this.searchboxIcon_}"
          .selectedMatch="${this.selectedMatch}"
          ?input-has-matches="${this.hasMatches()}"
          @focusin="${this.onInputFocusin_}"
          @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
          @input-focus-changed="${this.onInputFocusChanged}">
      </cr-searchbox-input>
      <omnibox-everywhere-profile-icon id="profileIcon"></omnibox-everywhere-profile-icon>
      <div class="dropdownContainer">
        <cr-searchbox-dropdown id="matches" part="searchbox-dropdown"
            exportparts="dropdown-content"
            role="listbox" .result="${this.result}"
            .selectedMatchIndex="${this.selectedMatchIndex}"
            @selected-match-index-changed="${this.onSelectedMatchIndexChanged}"
            @match-focusin="${this.onMatchFocusin}"
            @match-click="${this.onMatchClick}"
            ?hidden="${!this.dropdownIsVisible}">
        </cr-searchbox-dropdown>
      </div>
      <div id="bottomControls">
        <div id="actionButtons">
          ${
              this.showVoiceAndLensButtons_(
                  this.searchboxVoiceSearchEnabled_) ?
              html`
          <div class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                @click="${this.onVoiceSearchButtonClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
          ` :
              ''}
          ${this.isFuseboxEnabled &&
              this.showVoiceAndLensButtons_(
                  this.searchboxLensSearchEnabled_) ?
              html`
          <div class="searchbox-icon-button-container lens">
            <button id="lensSearchButton" class="searchbox-icon-button"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
          ` :
              ''}
        </div>
      </div>
      <cr-action-menu id="screenshotMenu" role-description="menu"
          @close="${this.onScreenshotMenuClose_}">
        <div class="menu-title">${this.i18n('shareScreenshotLabel')}</div>
        <button class="dropdown-item" id="screenshotFullscreen"
            @click="${this.onScreenshotEntireScreenClick_}">
          <div class="icon entire-screen"></div>
          ${this.i18n('screenshotEntireScreenLabel')}
        </button>
        <button class="dropdown-item" id="screenshotWindow"
            @click="${this.onScreenshotWindowClick_}">
          <div class="icon window"></div>
          ${this.i18n('screenshotWindowLabel')}
        </button>
        <button class="dropdown-item" id="screenshotRegion"
            @click="${this.onScreenshotRegionClick_}">
          <div class="icon region"></div>
          ${this.i18n('screenshotRegionLabel')}
        </button>
      </cr-action-menu>
    </div>
  `;
}
