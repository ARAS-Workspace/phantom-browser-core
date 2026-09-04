// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '/shared/settings/prefs/prefs.js';
import '../../controls/controlled_radio_button.js';
import '../../controls/settings_radio_group.js';
import '../../icons.html.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_page/settings_section.js';
import '../../settings_page/settings_subpage.js';
import './security_page_feature_row.js';
import './secure_dns.js';
import './secure_dns_v2.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {CrSettingsPrefs} from '/shared/settings/prefs/prefs_types.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ControlledRadioButtonElement} from '../../controls/controlled_radio_button.js';
import type {SettingsRadioGroupElement} from '../../controls/settings_radio_group.js';
import type {SettingsToggleButtonElement} from '../../controls/settings_toggle_button.js';
import {loadTimeData} from '../../i18n_setup.js';
import type {MetricsBrowserProxy} from '../../metrics_browser_proxy.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../../metrics_browser_proxy.js';
import {routes} from '../../route.js';
import type {Route} from '../../router.js';
import {RouteObserverMixin, Router} from '../../router.js';
import {SettingsViewMixin} from '../../settings_page/settings_view_mixin.js';
import {JavascriptOptimizerSetting} from '../../site_settings/constants.js';

import type {SettingsSecureDnsV2Element} from './secure_dns_v2.js';
import type {SecurityPageFeatureRowElement} from './security_page_feature_row.js';
import {getTemplate} from './security_page_v2.html.js';

/** Enumeration of all HTTPS-First Mode setting states.*/
// LINT.IfChange(HttpsFirstModeSetting)
export enum HttpsFirstModeSetting {
  DISABLED = 0,
  // DEPRECATED: A separate Incognito setting never shipped.
  // ENABLED_INCOGNITO = 1,
  ENABLED_FULL = 2,
  ENABLED_BALANCED = 3,
}
// LINT.ThenChange(/chrome/browser/ssl/https_first_mode_settings_tracker.h)

export interface SettingsSecurityPageV2Element {
  $: {
    blockForAllSites: ControlledRadioButtonElement,
    blockForUnfamiliarSites: ControlledRadioButtonElement,
    httpsFirstModeEnabledBalanced: ControlledRadioButtonElement,
    httpsFirstModeEnabledStrict: ControlledRadioButtonElement,
    httpsFirstModeRadioGroup: SettingsRadioGroupElement,
    httpsFirstModeRow: SecurityPageFeatureRowElement,
    httpsFirstModeToggle: SettingsToggleButtonElement,
    javascriptGuardrailsRow: SecurityPageFeatureRowElement,
    manageSiteExceptionsButton: CrButtonElement,
    secureDnsV2Row: SettingsSecureDnsV2Element,
  };
}

const SettingsSecurityPageV2ElementBase =
    RouteObserverMixin(SettingsViewMixin(PrefsMixin(PolymerElement)));

export class SettingsSecurityPageV2Element extends
    SettingsSecurityPageV2ElementBase {
  static get is() {
    return 'settings-security-page-v2';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      httpsFirstModeSettingEnum_: {
        type: Object,
        value: HttpsFirstModeSetting,
      },

      isHttpsFirstModeEnabled_: {
        type: Boolean,
        value: true,
      },

      httpsFirstModeUncheckedValues_: {
        type: Array,
        value: () => [HttpsFirstModeSetting.DISABLED],
      },

      javascriptGuardrailsOff_: {
        type: Array,
        value: () => [JavascriptOptimizerSetting.ALLOWED],
      },

      javascriptGuardrailsStateTextMap_: {
        type: Object,
        value: () => ({
          [JavascriptOptimizerSetting.BLOCKED_FOR_UNFAMILIAR_SITES]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [JavascriptOptimizerSetting.ALLOWED]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
          [JavascriptOptimizerSetting.BLOCKED]:
              loadTimeData.getString('securityFeatureRowStateEnhancedStrict'),
        }),
      },

      httpsFirstModeStateTextMap_: {
        type: Object,
        value: () => ({
          [HttpsFirstModeSetting.ENABLED_FULL]:
              loadTimeData.getString('securityFeatureRowStateEnhancedStrict'),
          [HttpsFirstModeSetting.ENABLED_BALANCED]:
              loadTimeData.getString('securityFeatureRowStateEnhanced'),
          [HttpsFirstModeSetting.DISABLED]:
              loadTimeData.getString('securityFeatureRowStateStandard'),
        }),
      },

      enableBundledSecuritySettingsSecureDnsV2_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('enableBundledSecuritySettingsSecureDnsV2'),
      },

      javascriptOptimizerSettingEnum_: {
        type: Object,
        value: JavascriptOptimizerSetting,
      },

      showNewBadge_: {
        type: Boolean,
        value: false,
        notify: true,
      },
    };
  }

  static get observers() {
    return [
      'updateRowsState_(prefs.generated.https_first_mode_enabled.*)',
    ];
  }

  // Keep in alphabetical order.
  declare private enableBundledSecuritySettingsSecureDnsV2_: boolean;
  declare private httpsFirstModeUncheckedValues_: HttpsFirstModeSetting[];
  declare private httpsFirstModeStateTextMap_: Object;
  declare private isHttpsFirstModeEnabled_: boolean;
  declare private javascriptGuardrailsOff_: JavascriptOptimizerSetting[];
  declare private javascriptGuardrailsStateTextMap_: Object;
  declare private showNewBadge_: boolean;

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();

  /**
   * RouteObserverMixin
   */
  override currentRouteChanged(route: Route) {
    if (route !== routes.SECURITY) {
      return;
    }

    const queryParams = Router.getInstance().getQueryParameters();
    const highlight = queryParams.get('highlight');
    if (highlight === 'secureConnections') {
      const row = this.shadowRoot!.querySelector('#httpsFirstModeRow');
      if (row) {
        row.classList.add('highlight');
        row.scrollIntoView({behavior: 'smooth', block: 'center'});
      }
    }
    CrSettingsPrefs.initialized.then(() => {
      // Capture the initial value of HFM bundle toast queued preference and set
      // the local showNewBadge_ property.
      const toastQueuedPref =
          this.getPref('https_first_mode_bundle_toast_queued');

      const isNew = toastQueuedPref ? !toastQueuedPref.value : true;
      this.showNewBadge_ = isNew;

      // Immediately mark the toast/badge as queued/shown so it is dismissed on
      // subsequent visits.
      this.setPrefValue('https_first_mode_bundle_toast_queued', true);
    });
  }

  private onHttpsFirstModeRowClick_(e: CustomEvent<{value: boolean}>) {
    const isExpanded = e.detail.value;
    if (isExpanded) {
      this.metricsBrowserProxy_.recordAction(
          'SafeBrowsing.Settings.HttpsFirstModeRowExpanded');
      this.setPrefValue('https_first_mode_bundle_toast_queued', true);
    }
  }

  private onManageCertificatesClick_() {
    this.metricsBrowserProxy_.recordSettingsPageHistogram(
        PrivacyElementInteractions.MANAGE_CERTIFICATES);
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('certManagementV2URL'));
  }

  private onManageSiteExceptionsClick_() {
    Router.getInstance().navigateTo(routes.SITE_SETTINGS_JAVASCRIPT_OPTIMIZER);
  }

  private updateRowsState_() {
    const httpsFirstModePref =
        this.getPref('generated.https_first_mode_enabled');
    this.isHttpsFirstModeEnabled_ = httpsFirstModePref.value !== undefined &&
        httpsFirstModePref.value !== HttpsFirstModeSetting.DISABLED;
  }

  // SettingsViewMixin implementation.
  override focusBackButton() {
    this.shadowRoot!.querySelector('settings-subpage')!.focusBackButton();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-security-page-v2': SettingsSecurityPageV2Element;
  }
}

customElements.define(
    SettingsSecurityPageV2Element.is, SettingsSecurityPageV2Element);
