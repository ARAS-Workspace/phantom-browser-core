// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file holds dependencies that are deliberately excluded
 * from the main app.js module, to force them to load after other app.js
 * dependencies. This is done to improve performance of initial rendering of
 * core elements of the landing page, by delaying loading of non-core
 * elements (either not visible by default or not as performance critical).
 */

import './modules/module_descriptors.js';
import './modules/modules.js';
import './ntp_promo/individual_promos.js';
import './ntp_promo/ntp_promo_proxy.js';
import './voice_search_overlay.js';
import 'chrome://resources/cr_components/most_visited/most_visited.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

export {CustomizeButtonsElement} from 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
export {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
export {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
export {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
export {PluralStringProxyImpl as NTPPluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy/foo_proxy.js';
export {dummyV2Descriptor, ModuleElement as DummyModuleElement} from './modules/dummy/module.js';
// </if>
export {InfoDialogElement} from './modules/info_dialog.js';
export {ModuleDescriptor} from './modules/module_descriptor.js';
export type {InitializeModuleCallback, Module} from './modules/module_descriptor.js';
export {counterfactualLoad} from './modules/module_descriptors.js';
export {ModuleHeaderElement as ModuleHeaderElementV2} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleWrapperElement} from './modules/module_wrapper.js';
export type {ModuleInstance} from './modules/module_wrapper.js';
export {ModulesElement, SUPPORTED_MODULE_WIDTHS} from './modules/modules.js';
export type {DisableModuleEvent, DismissModuleElementEvent, DismissModuleInstanceEvent, NamedWidth} from './modules/modules.js';
export {mostRelevantTabResumptionDescriptor, MostRelevantTabResumptionModuleElement} from './modules/most_relevant_tab_resumption/module.js';
export {MostRelevantTabResumptionProxyImpl} from './modules/most_relevant_tab_resumption/most_relevant_tab_resumption_proxy.js';
export {IconContainerElement} from './modules/tab_groups/icon_container.js';
export {COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX, COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX, colorIdToString, tabGroupsDescriptor, TabGroupsModuleElement} from './modules/tab_groups/module.js';
export {TabGroupsProxyImpl} from './modules/tab_groups/tab_groups_proxy.js';
export {IndividualPromosElement} from './ntp_promo/individual_promos.js';
export {NtpPromoProxyImpl} from './ntp_promo/ntp_promo_proxy.js';
export type {NtpPromoProxy} from './ntp_promo/ntp_promo_proxy.js';
export {VoiceSearchOverlayElement} from './voice_search_overlay.js';
