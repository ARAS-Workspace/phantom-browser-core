// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {UserEducationInternalsElement} from './user_education_internals.ts';

export function getHtml(this: UserEducationInternalsElement) {
  // clang-format off
  return html`
<cr-toolbar id="toolbar" page-name="IPH Tester"
    clear-label="Clear Search"
    search-prompt="Search IPH and Tutorials"
    autofocus
    @search-changed="${this.onSearchChanged_}"
    role="banner"
    @narrow-changed="${this.onNarrowChanged_}"
    narrow-threshold="920">
</cr-toolbar>
<div id="container" class="cr-scrollable">
  <div class="cr-scrollable-top-shadow"></div>
  <div id="left" ?hidden="${this.narrow_}">
    <div role="navigation">
      <h2>Navigation</h2>
      <cr-menu-selector id="menu" selectable="a" attr-for-selected="index"
          @iron-activate="${this.onSelectorIronActivate_}"
          @click="${this.onLinkClick_}"
          selected-attribute="selected">
        <a role="menuitem" index="-1" class="cr-nav-menu-item">
          Show All
        </a>
        <a role="menuitem" index="0" class="cr-nav-menu-item">
          Feature Promos
        </a>
        <a role="menuitem" index="1" class="cr-nav-menu-item">
          Tutorials
        </a>
        <a role="menuitem" index="2" class="cr-nav-menu-item">
          "New" Badges
        </a>
        <a role="menuitem" index="3" class="cr-nav-menu-item">
          Non-IPH Promos
        </a>
        <a role="menuitem" index="4" class="cr-nav-menu-item">
          NTP Promos
        </a>
        <a role="menuitem" index="5" class="cr-nav-menu-item">
          Session and Cooldown
        </a>
      </cr-menu-selector>
    </div>
  </div>
  <div id="main">
    <cr-toast id="errorMessageToast" duration="5000">
      <cr-icon id="errorMessageIcon" class="error-outline"
          icon="cr:error">
      </cr-icon>
      <span id="errorMessage">${this.featurePromoErrorMessage_}</span>
    </cr-toast>
    <div id="content">
      <div id="warning">
        <h2>User Education Debug Page</h2>
        <p>
          <span class="blurb-warning">NOTICE: DEBUGGING PAGE ONLY!</span>
          This page is not part of the intended Chrome experience. It is only
          for testing User Education features and may cause Chrome to behave
          in ways it was not designed to. Use at your own risk.
        </p>
      </div>
      <div id="uninitialized" ?hidden="${this.initialized_}">
        <h2>Waiting for Data...</h2>
        <p>
          Data is not yet loaded or the Feature Engagement system is not yet
          initialized. This page will populate when data is available.
        </p>
      </div>
      <cr-page-selector ?hidden="${!this.initialized_}"
                        .selected="${this.selectedTabIndex}" show-all="true"
                        id="selector">
        <div id="iph" class="promo-list">
          <h2>Feature Promos</h2>
          ${this.featurePromos_.map(item => html`
            <user-education-internals-card
                id="${item.internalName}"
                ?hidden="${!this.promoFilter_(item)}"
                .promo="${item}"
                show-action
                @promo-launch="${this.onFeaturePromoPromoLaunch_}"
                @clear-promo-data="${this.onFeaturePromoClearPromoData_}">
            </user-education-internals-card>`)}
          <p class="if-empty">
            No IPH match the search filter.
          </p>
        </div>
        <div id="tutorials" class="promo-list">
          <h2>Tutorials</h2>
          ${this.tutorials_.map(item => html`
            <user-education-internals-card
                id="${item.internalName}"
                ?hidden="${!this.promoFilter_(item)}"
                .promo="${item}"
                show-action
                @promo-launch="${this.onTutorialPromoLaunch_}">
            </user-education-internals-card>`)}
          <p class="if-empty">
            No Tutorials match the search filter.
          </p>
        </div>
        <div id="newBadges" class="promo-list">
          <h2>"New" Badges</h2>
          ${this.newBadges_.map(item => html`
            <user-education-internals-card
                id="${item.internalName}"
                ?hidden="${!this.promoFilter_(item)}"
                .promo="${item}"
                @clear-promo-data="${this.onNewBadgeClearPromoData_}">
            </user-education-internals-card>`)}
          <p class="if-empty">
            No "New" Badges match the search filter.
          </p>
        </div>
        <div id="nonIphPromos" class="promo-list">
          <h2>Non-IPH Promos</h2>
          ${this.nonIphPromos_.map(item => html`
            <user-education-internals-card
                id="${item.internalName}"
                ?hidden="${!this.promoFilter_(item)}"
                .promo="${item}"
                @clear-promo-data="${this.onNonIphClearPromoData_}">
            </user-education-internals-card>`)}
          <p class="if-empty">
            No non-IPH promos match the search filter.
          </p>
        </div>
        <div id="ntpPromos" class="promo-list">
          <h2>NTP Promos</h2>
          <div id="ntpPromoPreferences">
            <cr-expand-button
                ?expanded="${this.ntpPromoPreferencesExpanded_}"
                @expanded-changed="${this.onNtpPromoPreferencesExpandedChanged_}">
              <div id="label"><h3>NTP Promo Preferences</h3></div>
            </cr-expand-button>
            <div id="ntpPromoPrefData"
                ?hidden="${!this.ntpPromoPreferencesExpanded_}">
              ${this.ntpPromoPreferences_.map(item => html`
                <p><b>${item.name}</b> ${item.value}</p>`)}
              <p>
                Clicking the button below will reset all NTP promo preferences
                not tied to feature flags. These changes may not be reflected on
                NTP tabs that are already open.
              </p>
              <cr-button
                  id="clearNtpPromoPreferences"
                  @click="${this.onClearNtpPromoPreferencesClick_}">
                Clear All
              </cr-button>
            </div>
          </div>
          ${this.ntpPromos_.map(item => html`
            <user-education-internals-card
                id="${item.internalName}"
                ?hidden="${!this.promoFilter_(item)}"
                .promo="${item}"
                @clear-promo-data="${this.onNtpPromoClearPromoData_}">
            </user-education-internals-card>`)}
          <p class="if-empty">
            No NTP Promos match the search filter.
          </p>
        </div>
        <div id="global">
          <h2>Session, Grace Period and Cooldown</h2>
          <p>
            Sessions are used to track when a user starts to use the browser
            after a period of inactivity.
          </p>
          <p>
            In addition to tracking user activity, several grace periods and
            cooldowns apply that may prevent IPH and "New" Badges from
            displaying:
          </p>
          <ul>
            <li>
              <strong>New profile grace period:</strong>
              All low-priority IPH and "New" Badges are blocked for several
              days on any new profile.
            </li>
            <li>
              <strong>Session start grace period:</strong>
              Heavyweight IPH are blocked for several minutes after the user
              starts interacting with Chrome after significant time away.
            </li>
            <li>
              <strong>Heavyweight IPH cooldown:</strong>
              Heavyweight IPH are blocked for several days after the user
              interacts with another heavyweight IPH.
            </li>
          </ul>
          <div id="session">
            <cr-expand-button
                ?expanded="${this.sessionExpanded_}"
                @expanded-changed="${this.onSessionExpandedChanged_}">
              <div id="label">
                <h3>Session, Grace Period, and Cooldown data:</h3>
              </div>
            </cr-expand-button>
            <div id="sessionData" ?hidden="${!this.sessionExpanded_}">
              ${this.sessionData_.map(item => html`
                <p><b>${item.name}</b> ${item.value}</p>`)}
            </div>
            <p>
              <cr-button id="forceNewSession"
                  @click="${this.onForceNewSessionClick_}">
                Force New Session Right Now
              </cr-button>
            </p><p>
              <cr-button id="removeGracePeriods"
                  @click="${this.onRemoveGracePeriodsClick_}">
                Remove All Grace Periods And Cooldowns
              </cr-button>
            </p><p>
              <cr-button id="clearSession"
                  @click="${this.onClearSessionDataClick_}">
                Reset Profile State to New
              </cr-button>
            </p>
          </div>
        </div>
      </cr-page-selector>
    </div>
  </div>
  <div id="right" ?hidden="${this.narrow_}"></div>
</div>`;
  // clang-format on
}
