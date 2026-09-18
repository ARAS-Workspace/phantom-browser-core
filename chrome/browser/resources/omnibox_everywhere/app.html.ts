// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {OmniboxEverywhereAppElement} from './app.js';

export function getHtml(this: OmniboxEverywhereAppElement) {
  return html`<!--_html_template_start_-->
<div id="content">
  <omnibox-everywhere-omnibox id="searchbox"
      @open-voice-search="${this.onOpenVoiceSearch_}"
      .inVoiceSearchMode="${this.showVoiceSearchOverlay_}">
  </omnibox-everywhere-omnibox>
  ${this.mostVisitedEnabled_ ? html`
    <div id="mostVisitedContainer">
      <cr-most-visited id="mostVisited" single-row non-editable hide-title></cr-most-visited>
    </div>
  ` : ''}
</div>
<div id="dialogAnchor"></div>
<!--_html_template_end_-->`;
}
