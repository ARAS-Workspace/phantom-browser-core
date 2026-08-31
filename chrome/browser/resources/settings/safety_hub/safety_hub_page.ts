// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-hub-page' is the settings page that presents the safety
 * state of Chrome.
 */

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
// <if expr="not is_chromeos">
// </if>
import '../settings_page/settings_subpage.js';
import './safety_hub_card.js';
import './safety_hub_module.js';
import './extensions_module.js';
import './notification_permissions_module.js';
import './unused_site_permissions_module.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {MetricsBrowserProxy, SafetyHubCardState} from '../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, SafetyHubModuleType, SafetyHubSurfaces} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {Route} from '../router.js';
import {SettingsViewMixin} from '../settings_page/settings_view_mixin.js';

import type {CardInfo, NotificationPermission, SafetyHubBrowserProxy, UnusedSitePermissions} from './safety_hub_browser_proxy.js';
import {CardState, SafetyHubBrowserProxyImpl, SafetyHubEvent} from './safety_hub_browser_proxy.js';
import {getTemplate} from './safety_hub_page.html.js';

export interface SettingsSafetyHubPageElement {
  $: {
    safeBrowsing: HTMLElement,
  };
}

const SettingsSafetyHubPageElementBase = RouteObserverMixin(SettingsViewMixin(
    PrefsMixin(WebUiListenerMixin(I18nMixin(PolymerElement)))));

export class SettingsSafetyHubPageElement extends
    SettingsSafetyHubPageElementBase {
  static get is() {
    return 'settings-safety-hub-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // The object that holds data of Safe Browsing card.
      safeBrowsingCardData_: Object,

      // Whether Notification Permissions module should be visible.
      showNotificationPermissions_: {
        type: Boolean,
        value: false,
      },

      // Whether Unused Site Permissions module should be visible.
      showUnusedSitePermissions_: {
        type: Boolean,
        value: false,
      },

      // Whether Extensions module should be visible.
      showExtensions_: {
        type: Boolean,
        value: false,
      },

      showNoRecommendationsState_: {
        type: Boolean,
        computed:
            'computeShowNoRecommendationsState_(showUnusedSitePermissions_.*, showExtensions_.*, showNotificationPermissions_.*)',
      },


      // Whether the data for notification permissions is ready.
      hasDataForNotificationPermissions_: Boolean,

      // Whether the data for unused site permissions is ready.
      hasDataForUnusedPermissions_: Boolean,

      // Whether the data for extensions is ready.
      hasDataForExtensions_: Boolean,

    };
  }

  static get observers() {
    return [
      'onAllModulesLoaded_(safeBrowsingCardData_, hasDataForUnusedPermissions_, hasDataForNotificationPermissions_, hasDataForExtensions_)',
      'onSafeBrowsingPrefChanged_(prefs.generated.safe_browsing)',
    ];
  }

  declare private safeBrowsingCardData_: CardInfo;
  declare private showNotificationPermissions_: boolean;
  declare private hasDataForNotificationPermissions_: boolean;
  declare private showUnusedSitePermissions_: boolean;
  declare private hasDataForUnusedPermissions_: boolean;
  declare private showNoRecommendationsState_: boolean;
  declare private showExtensions_: boolean;
  declare private hasDataForExtensions_: boolean;
  private shouldRecordMetric_: boolean = false;
  private browserProxy_: SafetyHubBrowserProxy =
      SafetyHubBrowserProxyImpl.getInstance();
  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  override connectedCallback() {
    this.initializeCards_();
    this.initializeModules_();

    super.connectedCallback();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (Router.getInstance().getCurrentRoute() !== routes.SAFETY_HUB) {
      return;
    }
    // When the user navigates to the Safety Hub page, any active menu
    // notification is dismissed.
    this.browserProxy_.dismissActiveMenuNotification();

    // Record a visit to Safety Hub page if the user is still on the SH page
    // after 20 seconds.
    setTimeout(() => {
      if (Router.getInstance().getCurrentRoute() === routes.SAFETY_HUB) {
        this.browserProxy_.recordSafetyHubPageVisit();
      }
    }, 20000);

    this.metricsBrowserProxy_.recordSafetyHubImpression(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);
    this.metricsBrowserProxy_.recordSafetyHubInteraction(
        SafetyHubSurfaces.SAFETY_HUB_PAGE);

    // Only record the metrics when the user navigates to the Safety Hub page.
    this.shouldRecordMetric_ = true;
    this.onAllModulesLoaded_();
  }

  private initializeCards_() {
    this.browserProxy_.getSafeBrowsingCardData().then((data: CardInfo) => {
      this.safeBrowsingCardData_ = data;
    });
  }

  private initializeModules_() {
    this.addWebUiListener(
        SafetyHubEvent.NOTIFICATION_PERMISSIONS_MAYBE_CHANGED,
        (sites: NotificationPermission[]) =>
            this.onNotificationPermissionListChanged_(sites));

    this.addWebUiListener(
        SafetyHubEvent.UNUSED_PERMISSIONS_MAYBE_CHANGED,
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.addWebUiListener(
        SafetyHubEvent.EXTENSIONS_CHANGED,
        (num: number) => this.onExtensionsChanged_(num));

    this.browserProxy_.getNotificationPermissionReview().then(
        (sites: NotificationPermission[]) =>
            this.onNotificationPermissionListChanged_(sites));

    this.browserProxy_.getRevokedUnusedSitePermissionsList().then(
        (sites: UnusedSitePermissions[]) =>
            this.onUnusedSitePermissionListChanged_(sites));

    this.browserProxy_.getNumberOfExtensionsThatNeedReview().then(
        (num: number) => this.onExtensionsChanged_(num));
  }

  private onSafeBrowsingPrefChanged_() {
    this.browserProxy_.getSafeBrowsingCardData().then((data: CardInfo) => {
      this.safeBrowsingCardData_ = data;
    });
  }

  private onSafeBrowsingClick_() {
    this.metricsBrowserProxy_.recordSafetyHubCardStateClicked(
        'Settings.SafetyHub.SafeBrowsingCard.StatusOnClick',
        this.safeBrowsingCardData_.state as unknown as SafetyHubCardState);
    this.browserProxy_.recordSafetyHubInteraction();

    Router.getInstance().navigateTo(
        routes.SECURITY, /* dynamicParams= */ undefined,
        /* removeSearch= */ true);
  }

  private onSafeBrowsingKeyPress_(e: KeyboardEvent) {
    e.stopPropagation();
    if (this.isEnterOrSpaceClicked_(e)) {
      this.onSafeBrowsingClick_();
    }
  }

  private onNotificationPermissionListChanged_(permissions:
                                                   NotificationPermission[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showNotificationPermissions_ =
        permissions.length > 0 || this.showNotificationPermissions_;
    this.hasDataForNotificationPermissions_ = true;
  }

  private onUnusedSitePermissionListChanged_(permissions:
                                                 UnusedSitePermissions[]) {
    // The module should be visible if there is any item on the list, or if
    // there is no item on the list but the list was shown before.
    this.showUnusedSitePermissions_ =
        permissions.length > 0 || this.showUnusedSitePermissions_;
    this.hasDataForUnusedPermissions_ = true;
  }

  private computeShowNoRecommendationsState_(): boolean {
    return !(
        this.showUnusedSitePermissions_ || this.showNotificationPermissions_ ||
        this.showExtensions_);
  }

  private onExtensionsChanged_(numberOfExtensions: number) {
    this.showExtensions_ = !!numberOfExtensions;
    this.hasDataForExtensions_ = true;
  }

  private isEnterOrSpaceClicked_(e: KeyboardEvent): boolean {
    return e.key === 'Enter' || e.key === ' ';
  }

  private onAllModulesLoaded_() {
    // If the metrics are recorded already, don't record again.
    if (!this.shouldRecordMetric_) {
      return;
    }

    // Wait till the data of the cards be ready.
    if (!this.safeBrowsingCardData_) {
      return;
    }

    // Wait till the data of the modules be ready.
    if (!this.hasDataForUnusedPermissions_ ||
        !this.hasDataForNotificationPermissions_ ||
        !this.hasDataForExtensions_) {
      return;
    }

    this.shouldRecordMetric_ = false;
    let hasAnyWarning: boolean = false;
    // TODO(crbug.com/40267370): Iterate over the cards/modules with for loop.
    if (this.safeBrowsingCardData_.state !== CardState.SAFE) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.SAFE_BROWSING);
      hasAnyWarning = true;
    }

    if (this.showNotificationPermissions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.NOTIFICATIONS);
      hasAnyWarning = true;
    }

    if (this.showUnusedSitePermissions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.PERMISSIONS);
      hasAnyWarning = true;
    }

    if (this.showExtensions_) {
      this.metricsBrowserProxy_.recordSafetyHubModuleWarningImpression(
          SafetyHubModuleType.EXTENSIONS);
      hasAnyWarning = true;
    }

    this.metricsBrowserProxy_.recordSafetyHubDashboardAnyWarning(hasAnyWarning);
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-safety-hub-page': SettingsSafetyHubPageElement;
  }
}

customElements.define(
    SettingsSafetyHubPageElement.is, SettingsSafetyHubPageElement);
