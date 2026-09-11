// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/browser_prefs.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/android_buildflags.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/invert_bubble_prefs.h"
#include "chrome/browser/accessibility/page_colors_controller.h"
#include "chrome/browser/accessibility/prefers_default_scrollbar_styles_prefs.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/component_updater/component_updater_prefs.h"
#include "chrome/browser/context_hub/prefs.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/engagement/important_sites_util.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/finds/core/finds_service.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_prefs.h"
#include "chrome/browser/gpu/gpu_mode_manager.h"
#include "chrome/browser/hid/hid_policy_allowed_devices.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/login_detection/login_detection_prefs.h"
#include "chrome/browser/media/media_engagement_service.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "chrome/browser/media/prefs/capture_device_ranking.h"
#include "chrome/browser/media/unified_autoplay_config.h"
#include "chrome/browser/media/webrtc/capture_policy_utils.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/permission_bubble_media_access_handler.h"
#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/tab_stats/tab_stats_tracker.h"
#include "chrome/browser/net/net_error_tab_helper.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/notifications/notification_display_service_impl.h"
#include "chrome/browser/notifications/notifier_state_tracker.h"
#include "chrome/browser/notifications/platform_notification_service_impl.h"
#include "chrome/browser/permissions/quiet_notification_permission_ui_state.h"
#include "chrome/browser/platform_experience/prefs.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/preloading/prefetch/prefetch_service/prefetch_origin_decider.h"
#include "chrome/browser/preloading/prefetch/search_prefetch/search_prefetch_service.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/preloading/search_preload/search_preload_service.h"
#include "chrome/browser/printing/print_preview_sticky_settings.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/rlz/chrome_rlz_tracker_delegate.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/serial/serial_policy_allowed_ports.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/ssl/ssl_config_service_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/tips/core/tips_prefs.h"
#include "chrome/browser/tracing/chrome_tracing_delegate.h"
#include "chrome/browser/ui/browser_ui_prefs.h"
#include "chrome/browser/ui/network_profile_bubble.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tabs/organizer/organizer_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/toolbar/chrome_location_bar_model_delegate.h"
#include "chrome/browser/ui/toolbar/toolbar_pref_names.h"
#include "chrome/browser/ui/webui/accessibility/accessibility_ui.h"
#include "chrome/browser/ui/webui/bookmarks/bookmark_prefs.h"
#include "chrome/browser/ui/webui/flags/flags_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/updates/announcement_notification/announcement_notification_service.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/secure_origin_allowlist.h"
#include "components/accessibility_annotator/core/prefs.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/collaboration/public/pref_names.h"
#include "components/commerce/core/prefs.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"
#include "components/domain_reliability/domain_reliability_prefs.h"
#include "components/embedder_support/origin_trials/origin_trial_prefs.h"
#include "components/enterprise/browser/groups/groups_prefs.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/promotion/promotion_prefs.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/enterprise/network_header_injection/core/network_header_injection_prefs.h"
#include "components/feature_engagement/public/pref_names.h"
#include "components/history_clusters/core/history_clusters_prefs.h"
#include "components/image_fetcher/core/cache/image_cache.h"
#include "components/language/content/browser/geo_language_provider.h"
#include "components/language/content/browser/ulp_language_code_locator/ulp_language_code_locator.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/lens/buildflags.h"
#include "components/lookalikes/core/lookalike_url_util.h"
#include "components/media_device_salt/media_device_id_salt.h"
#include "components/metrics/demographics/user_demographics.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_choice_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ntp_tiles/custom_links_manager_impl.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcuts_manager_impl.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/document_provider.h"
#include "components/omnibox/browser/omnibox_prefs.h"
#include "components/omnibox/browser/zero_suggest_provider.h"
#include "components/on_device_translation/buildflags/buildflags.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/page_info/core/merchant_trust_service.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/payments/core/payment_prefs.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/permissions/permission_hats_trigger_helper.h"
#include "components/permissions/pref_names.h"
#include "components/personal_context/core/personal_context_prefs.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/policy/core/browser/url_list/url_list_policy_pref_names.h"
#include "components/policy/core/common/local_test_policy_provider.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/policy/core/common/policy_statistics_collector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safety_check/safety_check_prefs.h"
#include "components/saved_tab_groups/public/pref_names.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/security_interstitials/content/insecure_form_blocking_page.h"
#include "components/security_interstitials/content/stateful_ssl_host_state_delegate.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_result_dispatcher.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/sessions/core/session_id_generator.h"
#include "components/signin/core/browser/active_primary_accounts_metrics_recorder.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "components/skills/public/skills_prefs.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/core/common/constants.h"
#include "components/subscription_eligibility/subscription_eligibility_prefs.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/sync/base/account_pref_utils.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/service/device_statistics_scheduler.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "components/sync_preferences/cross_device_pref_tracker/prefs/cross_device_pref_registry.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/tracing/common/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/universal_optout/prefs.h"
#include "components/update_client/update_client.h"
#include "components/variations/service/variations_service.h"
#include "components/visited_url_ranking/internal/url_grouping/group_suggestions_service_impl.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "components/webui/flags/pref_service_flags_storage.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/buildflags.h"
#include "extensions/buildflags/buildflags.h"
#include "net/http/http_server_properties_manager.h"
#include "pdf/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "rlz/buildflags/buildflags.h"
#include "ui/webui/buildflags.h"

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_service.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption_page_handler.h"
#include "chrome/browser/ui/webui/cr_components/most_visited/most_visited_pref_observer.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
#include "chrome/browser/background/extensions/background_mode_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/preinstalled_extensions.h"
#include "chrome/browser/ui/webui/extensions/extensions_ui_prefs.h"
#include "extensions/browser/api/runtime/runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/pref_names.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/accessibility/animation_policy_prefs.h"
#include "chrome/browser/ui/extensions/extension_settings_overridden_dialog.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "extensions/browser/api/audio/audio_api.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
#include "chrome/browser/pdf/pdf_pref_names.h"
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/management/management_ui.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/accessibility/accessibility_prefs/android/accessibility_prefs_controller.h"
#include "chrome/browser/android/ntp/recent_tabs_page_prefs.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/android/preferences/browser_prefs_android.h"
#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"
#include "chrome/browser/android/usage_stats/usage_stats_bridge.h"
#include "chrome/browser/auxiliary_search/auxiliary_search_donation_service.h"
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "chrome/browser/lens/android/lens_prefs.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/notifications/notification_channels_provider_android.h"
#include "chrome/browser/ntp_customization/ntp_android_custom_background_service.h"
#include "chrome/browser/partnerbookmarks/partner_bookmarks_shim.h"
#include "chrome/browser/readaloud/android/prefs.h"
#include "chrome/browser/ssl/known_interception_disclosure_infobar_delegate.h"
#include "chrome/browser/ui/safety_hub/safety_hub_prefs.h"
#include "components/cdm/browser/media_drm_storage_impl.h"  // nogncheck crbug.com/40147906
#include "components/feed/core/common/pref_names.h"        // nogncheck
#include "components/feed/core/shared_prefs/pref_names.h"  // nogncheck
#include "components/feed/core/v2/ios_shared_prefs.h"      // nogncheck
#include "components/ntp_tiles/popular_sites_impl.h"
#include "components/permissions/contexts/geolocation_permission_context_android.h"
#include "components/webapps/browser/android/install_prompt_prefs.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/ui/actor_ui_state_manager_prefs.h"
#include "chrome/browser/desktop_to_mobile_promos/promos_utils.h"  // nogncheck crbug.com/40147906
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/intranet_redirect_detector.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar_page_handler.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups_page_handler.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/screen_ai/pref_names.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/themes/theme_syncable_service.h"
#include "chrome/browser/ui/commerce/commerce_ui_tab_helper.h"
#include "chrome/browser/ui/hats/hats_service_desktop.h"
#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble.h"
#include "chrome/browser/ui/side_panel/side_panel_prefs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/tabs/pinned_tab_codec.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_pref_names.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_handler.h"
#include "chrome/browser/ui/webui/cr_components/theme_color_picker/theme_color_picker_handler.h"
#include "chrome/browser/ui/webui/history/foreign_session_handler.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_prefs.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/browser/user_education/browser_user_education_storage_service.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "components/headless/policy/headless_mode_prefs.h"  // nogncheck crbug.com/40147906
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/live_caption/live_caption_controller.h"
#include "components/live_caption/live_translate_controller.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
#include "chrome/browser/devtools/devtools_window.h"
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/media/webrtc/system_media_capture_permissions_stats_mac.h"
#include "chrome/browser/ui/cocoa/apps/quit_with_apps_controller_mac.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/browser/ui/views/frame/glass_frame_service.h"
#include "chrome/browser/web_applications/os_integration/mac/app_shim_registry.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "components/device_signals/core/browser/pref_names.h"  // nogncheck due to crbug.com/40147906
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_prefs.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/startup/first_run_service.h"
#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
#include "chrome/browser/downgrade/downgrade_prefs.h"  // nogncheck
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/device_identity/device_oauth2_token_store_desktop.h"
#include "chrome/browser/ui/startup/default_browser_prompt/default_browser_prompt.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/browser_view_prefs.h"
#include "chrome/browser/ui/side_search/side_search_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_data_service.h"
#include "chrome/browser/sessions/session_service_log.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/color/system_theme.h"
#endif

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "components/on_device_translation/public/pref_names.h"
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
#include "components/enterprise/data_controls/core/browser/prefs.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "components/safe_browsing/content/common/file_type_policies_prefs.h"
#endif

#if BUILDFLAG(CHROME_FOR_TESTING)
#include "chrome/browser/chrome_for_testing/prefs.h"
#endif

namespace {

// Please keep the list of deprecated prefs in chronological order. i.e. Add to
// the bottom of the list, not here at the top.

// Deprecated 08/2025.
inline constexpr char kInvalidationClientIDCache[] =
    "invalidation.per_sender_client_id_cache";
inline constexpr char kInvalidationTopicsToHandler[] =
    "invalidation.per_sender_topics_to_handler";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 08/2025.
constexpr char kObsoleteAccountStorageNoticeShown[] =
    "password_manager.account_storage_notice_shown";
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
// Deprecated 08/2025.
constexpr char kObsoleteAutofillableCredentialsProfileStoreLoginDatabase[] =
    "password_manager.autofillable_credentials_profile_store_login_database";
constexpr char kObsoleteAutofillableCredentialsAccountStoreLoginDatabase[] =
    "password_manager.autofillable_credentials_account_store_login_database";
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
// Deprecated 09/2025.
constexpr char kObsoleteUpmUnmigratedPasswordsExported[] =
    "profile.upm_unmigrated_passwords_exported";
constexpr char kObsoletePasswordsUseUPMLocalAndSeparateStores[] =
    "passwords_use_upm_local_and_separate_stores";
constexpr char kObsoleteEmptyProfileStoreLoginDatabase[] =
    "password_manager.empty_profile_store_login_database";
constexpr char kObsoleteUpmAutoExportCsvNeedsDeletion[] =
    "profile.upm_auto_export_csv_needs_deletion";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 09/2025.
constexpr char kGaiaCookieLastListAccountsData[] =
    "gaia_cookie.last_list_accounts_data";

// Deprecated 09/2025.
constexpr char kLensOverlayEduActionChipShownCount[] =
    "lens.edu_action_chip.shown_count";

constexpr char kRendererCodeIntegrityEnabledNeedsDeletion[] =
    "renderer_code_integrity_enabled";

// Deprecated 10/2025.
constexpr char kSessionRestoreTurnOffFromRestartInfoBarTimesShown[] =
    "browser.session_restore_turn_off_from_restart_infobar_times_shown";

constexpr char kSessionRestoreTurnOffFromSessionInfoBarTimesShown[] =
    "browser.session_restore_turn_off_from_session_infobar_times_shown";

constexpr char kSessionRestorePrefChanged[] = "session.restore_pref_changed";

constexpr char kLegacySyncSessionsGUID[] = "sync.session_sync_guid";

const char kRefreshHeuristicBreakageException[] =
    "fingerprinting_protection_filter.refresh_heuristic_breakage_exception_"
    "sites";

const char kFpfRulesetContent[] =
    "fingerprinting_protection_filter.ruleset_version.content";

const char kFpfRulesetFormat[] =
    "fingerprinting_protection_filter.ruleset_version.format";

const char kFpfRulesetChecksum[] =
    "fingerprinting_protection_filter.ruleset_version.checksum";

// Deprecated 12/2025.
const char kPrivacyBudgetGeneration[] = "privacy_budget.generation";
const char kPrivacyBudgetSeenSurfaces[] = "privacy_budget.seen";
const char kPrivacyBudgetSelectedOffsets[] = "privacy_budget.selected";
const char kPrivacyBudgetSelectedBlock[] = "privacy_budget.block_offset";
const char kPrivacyBudgetMetaExperimentActivationSalt[] =
    "privacy_budget.meta_experiment_activation_salt";

// Preference key for Enterprise policy UserAgentReduction which is distinct
// from blink::features::kReduceUserAgentMinorVersion.
constexpr char kReduceUserAgentMinorVersion[] = "user_agent_reduction";

// Deprecated 12/2025.
constexpr char kAutofillStatesDataDir[] = "autofill.states_data_dir";
constexpr char kMerchantTrustUiLastInteractionTime[] =
    "merchant_trust.ui.last_interaction_time";
constexpr char kMerchantTrustPageInfoLastOpenTime[] =
    "merchant_trust.page_info.last_open_time";

// Deprecated 12/2025.
constexpr char kCloudPrintProxyEnabled[] = "cloud_print.enabled";
constexpr char kCloudPrintEmail[] = "cloud_print.email";

// Deprecated 12/2025.
constexpr char kTrackingProtectionEligibleSince[] =
    "tracking_protection.tracking_protection_eligible_since";
constexpr char kTrackingProtectionOnboardedSince[] =
    "tracking_protection.tracking_protection_onboarded_since";
constexpr char kTrackingProtectionNoticeLastShown[] =
    "tracking_protection.tracking_protection_notice_last_shown";
constexpr char kTrackingProtectionOnboardingAckedSince[] =
    "tracking_protection.tracking_protection_onboarding_acked_since";
constexpr char kTrackingProtectionOnboardingAcked[] =
    "tracking_protection.tracking_protection_onboarding_acked";
constexpr char kTrackingProtectionOnboardingAckAction[] =
    "tracking_protection.tracking_protection_onboarding_ack_action";
constexpr char kTrackingProtectionSilentEligibleSince[] =
    "tracking_protection.tracking_protection_silent_eligible_since";
constexpr char kTrackingProtectionSilentOnboardedSince[] =
    "tracking_protection.tracking_protection_silent_onboarded_since";
constexpr char kAllowAll3pcToggleEnabled[] =
    "tracking_protection.allow_all_3pc_toggle_enabled";
constexpr char kTrackingProtectionLevel[] =
    "tracking_protection.tracking_protection_level";
constexpr char kIpProtectionEnabled[] =
    "tracking_protection.ip_protection_enabled";
constexpr char kIpProtectionInitializedByDogfood[] =
    "tracking_protection.ip_protection_initialized_by_dogfood";
constexpr char kUserBypass3pcExceptionsMigrated[] =
    "tracking_protection.user_bypass_3pc_exceptions_migrated";
constexpr char kTrackingProtectionSilentOnboardingStatus[] =
    "tracking_protection.tracking_protection_silent_onboarding_status";
constexpr char kFingerprintingProtectionEnabled[] =
    "tracking_protection.fingerprinting_protection_enabled";
constexpr char kTrackingProtectionOnboardingStatus[] =
    "tracking_protection.tracking_protection_onboarding_status";
constexpr char kTPCDExperimentClientState[] = "tpcd_experiment.client_state";
constexpr char kTPCDExperimentClientStateVersion[] =
    "tpcd_experiment.client_state_version";
constexpr char kTPCDExperimentProfileState[] = "tpcd_experiment.profile_state";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 01/2026.
constexpr char kDSEGeolocationSettingDeprecated[] = "dse_geolocation_setting";
constexpr char kDSEPermissionsSettings[] = "dse_permissions_settings";
constexpr char kDSEWasDisabledByPolicy[] = "dse_was_disabled_by_policy";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 01/2026.
constexpr char kCookieClearOnExitMigrationNoticeComplete[] =
    "signin.cookie_clear_on_exit_migration_notice_complete";

// Deprecated 02/2026.
// Note that these were replaced by local state prefs of the same names and
// functions: those should not be removed. Only the profile prefs registered
// here are deprecated.
constexpr char kGlicGuestUrlPresetAutopush[] = "glic.guest_url_preset_autopush";
constexpr char kGlicGuestUrlPresetPreprod[] = "glic.guest_url_preset_preprod";
constexpr char kGlicGuestUrlPresetProd[] = "glic.guest_url_preset_prod";

// Deprecated 02/2026.
constexpr char kProfilesDeletedOld[] = "profiles.profiles_deleted";

// Deprecated 02/2026.
inline constexpr char kExplicitBrowserSigninWithoutFeatureEnabled[] =
    "signin.explicit_browser_signin";

// Deprecated 02/2026.
constexpr char kDiceMigrationDialogShownCount[] =
    "signin.dice_migration.dialog_shown_count";
constexpr char kDiceMigrationDialogLastShownTime[] =
    "signin.dice_migration.dialog_last_shown_time";
constexpr char kDiceMigrationBackup[] = "signin.dice_migration.backup";
constexpr char kDiceMigrationRestoredFromBackup[] =
    "signin.dice_migration.restored_from_backup";

// Deprecated 02/2026.
inline constexpr char kTabSearchOpened[] = "tab_search.opened";

// Deprecated 02/2026.
constexpr char kTabOrganizationFeature[] = "tab_organization.feature";

// Deprecated 03/2026.
constexpr char kTabDeclutterUsageCount[] = "tab_declutter.usage_count";

// Deprecated 03/2026.
inline constexpr char kTabSearchTabIndex[] = "tab_search.tab_index";

// Deprecated 03/2026.
constexpr char kGlicMultiInstanceEnabledBySubscriptionTier[] =
    "glic.multi_instance_enabled_by_tier";

// Deprecated 03/2026
constexpr char kSigninFromBookmarksBubbleSyntheticTrialGroupNamePref[] =
    "UnoDesktopBookmarksEnabledInAccountFromBubbleGroup";
constexpr char kBookmarksBubblePromoShownSyntheticTrialGroupNamePref[] =
    "UnoDesktopBookmarksBubblePromoShownGroup";

#if BUILDFLAG(IS_ANDROID)
// Deprecated 03/2026.
constexpr char kPrivacySandboxActivityTypeRecord2[] =
    "privacy_sandbox.activity_type.record2";
#endif  // BUILDFLAG(IS_ANDROID)

// Deprecated 03/2026.
constexpr char kTabOrganizationNudgeBackoffCount[] =
    "tab_organization.nudge_backoff_count";
constexpr char kTabOrganizationShowFRE[] = "tab_organization.show_fre_2";
constexpr char kTabOrganizationModelStrategy[] =
    "tab_organization.model_strategy";

// Deprecated 03/2026.
constexpr char kNtpContextMenuClickCount[] = "ntp.context_menu_click_count";

// Deprecated 03/2026.
constexpr char kNtpPromoPrefLastSnoozed[] =
    "in_product_help.ntp_promos.last_snoozed";

// Deprecated 03/2026.
constexpr char kSafeBrowsingModuleShownCount[] =
    "safebrowsing.ntp.module_shown_count";
constexpr char kSafeBrowsingModuleLastCooldownStartAt[] =
    "safebrowsing.ntp.last_cooldown_start_timestamp";
constexpr char kSafeBrowsingModuleOpened[] =
    "safebrowsing.ntp.user_opened_module";

// Deprecated 04/2026.
constexpr char kTpcdMetadataCohorts[] = "tpcd.metadata.cohorts";

// Deprecated 04/2026.
constexpr char kHasSeenWebFeed[] = "webfeed.has_seen_feed";
constexpr char kLastBadgeAnimationTime[] = "webfeed.last_badge_animation_time";

// Deprecated 04/2026.
inline constexpr char kPreallocatedAddressesVersion[] =
    "plus_addresses.preallocation.version";
inline constexpr char kPreallocatedAddresses[] =
    "plus_addresses.preallocation.addresses";
inline constexpr char kPreallocatedAddressesNext[] =
    "plus_addresses.preallocation.next";
inline constexpr char kFirstPlusAddressCreationTime[] =
    "plus_addresses.creation.first.time";
inline constexpr char kLastPlusAddressFillingTime[] =
    "plus_addresses.last.filling.time";

// Deprecated 05/2026.
constexpr char kWebFeedContentOrder[] = "webfeed.content_order";
constexpr char kWebFeedsRequestSchedule[] = "webfeed.request_schedule";
constexpr char kEnableWebFeedFollowIntroDebug[] =
    "webfeed_follow_intro_debug.enable";
constexpr char kLastSeenFeedType[] = "feedv2.last_seen_feed_type";

// Deprecated 05/2026.
constexpr char kShouldShowRemoteAnnotatorFirstRunInfo[] =
    "accessibility_annotator.should_show_remote_annotator_first_run_info";

// Deprecated 05/2026.
constexpr char kHttpCacheFinchExperimentGroups[] =
    "profile_network_context_service.http_cache_finch_experiment_groups";

// Deprecated 05/2026.
constexpr char kContextualCueingEnterprisePolicyAllowedDeprecated[] =
    "optimization_guide.model_execution.contextual_cueing_enterprise_policy_"
    "allowed";

// Deprecated 06/2026.
inline constexpr char kDeleteTimePeriodBasic[] =
    "browser.clear_data.time_period_basic";
inline constexpr char kDeleteBrowsingHistoryBasic[] =
    "browser.clear_data.browsing_history_basic";
inline constexpr char kDeleteCacheBasic[] = "browser.clear_data.cache_basic";
inline constexpr char kDeleteCookiesBasic[] =
    "browser.clear_data.cookies_basic";
inline constexpr char kLastClearBrowsingDataTab[] =
    "browser.last_clear_browsing_data_tab";

// Deprecated 06/2026.
constexpr char kGlicSelectionWidgetDismissCount[] =
    "glic.selection_widget_dismiss_count";

// Deprecated 06/2026.
inline constexpr char kRealboxContextMenuAnimationState[] =
    "realbox.context_menu_animation_state";

// Deprecated 06/2026.
inline constexpr char kTabSearchMigrationComplete[] =
    "toolbar.tab_search_migration_complete";
inline constexpr char kTabSearchPinnedToTabstripMigrationComplete[] =
    "tab_search.pinned_to_tabstrip_migration_complete";
inline constexpr char kTabSearchPinnedToTabstripMigrationComplete2[] =
    "tab_search.pinned_to_tabstrip_migration_complete_2";

// Deprecated 06/2026.
constexpr char kPersonalContextInAutofillNoticeShouldBeShown[] =
    "autofill.personal_context.notice_should_be_shown";

// Deprecated 06/2026.
inline constexpr char kDefaultBrowserInfobarLastDeclined[] =
    "browser.default_browser_infobar_last_declined";

// Deprecated 07/2026.
inline constexpr char kObsoleteMetricsReportingLevel[] =
    "user_experience_metrics.reporting_level";
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
inline constexpr char kProxyOverrideRulesAffiliation[] =
    "proxy_override_rules_affiliation";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// Deprecated 07/2026.
inline constexpr char kMV2DeprecationWarningAcknowledgedGlobally[] =
    "mv2_deprecation_warning_ack_globally";
inline constexpr char kMV2DeprecationDisabledAcknowledgedGlobally[] =
    "mv2_deprecation_disabled_ack_globally";
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

// Deprecated 07/2026.
inline constexpr char kObsoleteManagementPlatformLastLogTime[] =
    "management.platform.last_log_time";
inline constexpr char kObsoleteManagementProfileLastLogTime[] =
    "management.profile.last_log_time";

// Deprecated 07/2026.
constexpr char kMetricsReportingMigrationDone[] =
    "user_experience_metrics.consent_migration_done";
constexpr char kMetricsConsentRestructureFeatureState[] =
    "user_experience_metrics.consent_restructure_feature_state";

// Deprecated 08/2026.
constexpr char kPrivacySandboxNotices[] = "privacy_sandbox.notices";
constexpr char kPrivacySandboxM1ConsentDecisionMade[] =
    "privacy_sandbox.m1.consent_decision_made";
constexpr char kPrivacySandboxM1EEANoticeAcknowledged[] =
    "privacy_sandbox.m1.eea_notice_acknowledged";
constexpr char kPrivacySandboxM1RowNoticeAcknowledged[] =
    "privacy_sandbox.m1.row_notice_acknowledged";
constexpr char kPrivacySandboxM1RestrictedNoticeAcknowledged[] =
    "privacy_sandbox.m1.restricted_notice_acknowledged";
constexpr char kPrivacySandboxM1PromptSuppressed[] =
    "privacy_sandbox.m1.prompt_suppressed";
constexpr char kPrivacySandboxNoticeDisplayed[] =
    "privacy_sandbox.notice_displayed";
constexpr char kPrivacySandboxConsentDecisionMade[] =
    "privacy_sandbox.consent_decision_made";
constexpr char kPrivacySandboxNoConfirmationSandboxDisabled[] =
    "privacy_sandbox.no_confirmation_sandbox_disabled";
constexpr char kPrivacySandboxNoConfirmationSandboxRestricted[] =
    "privacy_sandbox.no_confirmation_sandbox_restricted";
constexpr char kPrivacySandboxNoConfirmationSandboxManaged[] =
    "privacy_sandbox.no_confirmation_sandbox_managed";
constexpr char kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked[] =
    "privacy_sandbox.no_confirmation_3PC_blocked";
constexpr char kPrivacySandboxNoConfirmationManuallyControlled[] =
    "privacy_sandbox.no_confirmation_manually_controlled";
constexpr char kPrivacySandboxDisabledInsufficientConfirmation[] =
    "privacy_sandbox.disabled_insufficient_confirmation";
constexpr char kPrivacySandboxTopicsConsentGiven[] =
    "privacy_sandbox.topics_consent.consent_given";
constexpr char kPrivacySandboxTopicsConsentLastUpdateTime[] =
    "privacy_sandbox.topics_consent.last_update_time";
constexpr char kPrivacySandboxTopicsConsentLastUpdateReason[] =
    "privacy_sandbox.topics_consent.last_update_reason";
constexpr char kPrivacySandboxTopicsConsentTextAtLastUpdate[] =
    "privacy_sandbox.topics_consent.text_at_last_update";
constexpr char kPrivacySandboxAllowNoticeFor3PCBlockedTrial[] =
    "privacy_sandbox.allow_notice_for_3PC_blocked_trial";
constexpr char kObsoleteAutofillWalletImportEnabled[] =
    "autofill.wallet_import_enabled";
constexpr char kObsoleteAutofillWalletImportEnabledMigrated[] =
    "sync.autofill_wallet_import_enabled_migrated";
constexpr char kPrivacySandboxTopicsDataAccessibleSince[] =
    "privacy_sandbox.topics_data_accessible_since";
constexpr char kPrivacySandboxBlockedTopics[] =
    "privacy_sandbox.blocked_topics";
constexpr char kPrivacySandboxFledgeJoinBlocked[] =
    "privacy_sandbox.fledge_join_blocked";
constexpr char kShowRollbackUiModeB[] =
    "tracking_protection.show_rollback_ui_mode_b";
constexpr char kTrackingProtection3pcdEnabled[] =
    "tracking_protection.tracking_protection_3pcd_enabled";
constexpr char kBlockAll3pcToggleEnabled[] =
    "tracking_protection.block_all_3pc_toggle_enabled";

// Register local state used only for migration (clearing or moving to a new
// key).
void RegisterLocalStatePrefsForMigration(PrefRegistrySimple* registry) {
  // Deprecated 08/2025.
  registry->RegisterDictionaryPref(kInvalidationClientIDCache);
  registry->RegisterDictionaryPref(kInvalidationTopicsToHandler);

  // Deprecated 09/2025.
  registry->RegisterBooleanPref(kRendererCodeIntegrityEnabledNeedsDeletion,
                                false);

  // Deprecated 11/2025.
  registry->RegisterStringPref(kFpfRulesetContent, std::string());
  registry->RegisterIntegerPref(kFpfRulesetFormat, 0);
  registry->RegisterUint64Pref(kFpfRulesetChecksum, 0);

  // Deprecated 12/2025.
  registry->RegisterIntegerPref(kPrivacyBudgetGeneration, 0);
  registry->RegisterStringPref(kPrivacyBudgetSeenSurfaces, std::string());
  registry->RegisterStringPref(kPrivacyBudgetSelectedOffsets, std::string());

  // Deprecated 03/2026.
  registry->RegisterBooleanPref(kGlicMultiInstanceEnabledBySubscriptionTier,
                                false);
  registry->RegisterIntegerPref(kPrivacyBudgetSelectedBlock, -1);
  registry->RegisterDoublePref(kPrivacyBudgetMetaExperimentActivationSalt, 0);

  // Deprecated 12/2025.
  registry->RegisterStringPref(kAutofillStatesDataDir, std::string());

  // Deprecated 12/2025.
  registry->RegisterBooleanPref(kFingerprintingProtectionEnabled, false);
  registry->RegisterBooleanPref(kIpProtectionEnabled, false);
  registry->RegisterBooleanPref(kAllowAll3pcToggleEnabled, false);
  registry->RegisterBooleanPref(kUserBypass3pcExceptionsMigrated, false);
  registry->RegisterIntegerPref(kTrackingProtectionLevel, 0);
  registry->RegisterTimePref(kTrackingProtectionSilentOnboardedSince,
                             base::Time());
  registry->RegisterTimePref(kTrackingProtectionSilentEligibleSince,
                             base::Time());
  registry->RegisterTimePref(kTrackingProtectionEligibleSince, base::Time());
  registry->RegisterTimePref(kTrackingProtectionOnboardedSince, base::Time());
  registry->RegisterTimePref(kTrackingProtectionNoticeLastShown, base::Time());
  registry->RegisterTimePref(kTrackingProtectionOnboardingAckedSince,
                             base::Time());
  registry->RegisterBooleanPref(kTrackingProtectionOnboardingAcked, false);
  registry->RegisterIntegerPref(kTrackingProtectionOnboardingAckAction, 0);
  registry->RegisterBooleanPref(kIpProtectionInitializedByDogfood, false);
  registry->RegisterIntegerPref(kTrackingProtectionSilentOnboardingStatus, 0);
  registry->RegisterIntegerPref(kTrackingProtectionOnboardingStatus, 0);
  registry->RegisterIntegerPref(kTPCDExperimentClientState, 0);
  registry->RegisterIntegerPref(kTPCDExperimentClientStateVersion, 0);
  registry->RegisterIntegerPref(kTPCDExperimentProfileState, 0);

  // Deprecated 02/2026.
  registry->RegisterListPref(kProfilesDeletedOld);

  // Deprecated 04/2026.
  registry->RegisterDictionaryPref(kTpcdMetadataCohorts);

  // Deprecated 04/2026.
  registry->RegisterBooleanPref(kHasSeenWebFeed, false);
  registry->RegisterTimePref(kLastBadgeAnimationTime, base::Time());

  // Deprecated 05/2026.
  registry->RegisterStringPref(kHttpCacheFinchExperimentGroups, "");

  // Deprecated 07/2026.
  registry->RegisterIntegerPref(kObsoleteMetricsReportingLevel, 0);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kProxyOverrideRulesAffiliation, true);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  registry->RegisterBooleanPref(kMetricsReportingMigrationDone, false);
  registry->RegisterBooleanPref(kMetricsConsentRestructureFeatureState, false);

  // Deprecated 07/2026.
  registry->RegisterTimePref(kObsoleteManagementPlatformLastLogTime,
                             base::Time());
}

// Register prefs used only for migration (clearing or moving to a new key).
void RegisterProfilePrefsForMigration(
    user_prefs::PrefRegistrySyncable* registry) {
  // Deprecated 08/2025.
  registry->RegisterDictionaryPref(kInvalidationClientIDCache);
  registry->RegisterDictionaryPref(kInvalidationTopicsToHandler);

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 08/2025.
  registry->RegisterBooleanPref(kObsoleteAccountStorageNoticeShown, false);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 08/2025.
  registry->RegisterBooleanPref(
      kObsoleteAutofillableCredentialsProfileStoreLoginDatabase, false);
  registry->RegisterBooleanPref(
      kObsoleteAutofillableCredentialsAccountStoreLoginDatabase, false);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_WEBUI_NTP)
  // Deprecated 08/2025.
  registry->RegisterBooleanPref(ntp_prefs::kNtpUseMostVisitedTiles, false);
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 09/2025.
  registry->RegisterBooleanPref(kObsoleteUpmUnmigratedPasswordsExported, false);
  registry->RegisterIntegerPref(kObsoletePasswordsUseUPMLocalAndSeparateStores,
                                0);
  registry->RegisterBooleanPref(kObsoleteEmptyProfileStoreLoginDatabase, false);
  registry->RegisterBooleanPref(kObsoleteUpmAutoExportCsvNeedsDeletion, false);
#endif  // BUILDFLAG(IS_ANDROID)

  // Deprecated 09/2025.
  registry->RegisterStringPref(kGaiaCookieLastListAccountsData, std::string());

  // Deprecated 09/2025.
  registry->RegisterIntegerPref(kLensOverlayEduActionChipShownCount, 0);

  // Deprecated 10/2025.
  registry->RegisterIntegerPref(
      kSessionRestoreTurnOffFromRestartInfoBarTimesShown, 0);

  // Deprecated 10/2025.
  registry->RegisterIntegerPref(
      kSessionRestoreTurnOffFromSessionInfoBarTimesShown, 0);

  // Deprecated 10/2025.
  registry->RegisterBooleanPref(kSessionRestorePrefChanged, false);

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_WEBUI_NTP)
  // Deprecated 10/2025.
  registry->RegisterIntegerPref(ntp_prefs::kNtpShortcutsType, 0);
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_WEBUI_NTP)

  // Deprecated 10/2025.
  registry->RegisterStringPref(kLegacySyncSessionsGUID, std::string());

  // Deprecated 11/2025.
  registry->RegisterDictionaryPref(kRefreshHeuristicBreakageException);

  // Deprecated 12/2025.
  registry->RegisterBooleanPref(kReduceUserAgentMinorVersion, false);
  registry->RegisterTimePref(kMerchantTrustUiLastInteractionTime, base::Time());
  registry->RegisterTimePref(kMerchantTrustPageInfoLastOpenTime, base::Time());

  // Deprecated 12/2025.
  registry->RegisterBooleanPref(kCloudPrintProxyEnabled, true);
  registry->RegisterStringPref(kCloudPrintEmail, std::string());

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 01/2026.
  registry->RegisterDictionaryPref(kDSEGeolocationSettingDeprecated);
  registry->RegisterDictionaryPref(kDSEPermissionsSettings);
  registry->RegisterBooleanPref(kDSEWasDisabledByPolicy, false);
#endif  // BUILDFLAG(IS_ANDROID)

  // Deprecated 01/2026.
  registry->RegisterBooleanPref(kCookieClearOnExitMigrationNoticeComplete,
                                false);

  // Deprecated 02/2026.
  registry->RegisterStringPref(kGlicGuestUrlPresetAutopush, std::string());
  registry->RegisterStringPref(kGlicGuestUrlPresetPreprod, std::string());
  registry->RegisterStringPref(kGlicGuestUrlPresetProd, std::string());

  // Deprecated 02/2026.
  registry->RegisterBooleanPref(kExplicitBrowserSigninWithoutFeatureEnabled,
                                false);

  // Deprecated 02/2026.
  registry->RegisterIntegerPref(kDiceMigrationDialogShownCount, 0);
  registry->RegisterTimePref(kDiceMigrationDialogLastShownTime, base::Time());
  registry->RegisterDictionaryPref(kDiceMigrationBackup);
  registry->RegisterBooleanPref(kDiceMigrationRestoredFromBackup, false);

  // Deprecated 02/2026.
  registry->RegisterBooleanPref(kTabSearchOpened, false);

  // Deprecated 02/2026.
  registry->RegisterIntegerPref(kTabOrganizationFeature, 0);

  // Deprecated 03/2026.
  registry->RegisterIntegerPref(kTabDeclutterUsageCount, 0);

  // Deprecated 03/2026.
  registry->RegisterIntegerPref(kTabSearchTabIndex, 1);

  // Deprecated 03/2026.
  registry->RegisterStringPref(
      kSigninFromBookmarksBubbleSyntheticTrialGroupNamePref, std::string());
  registry->RegisterStringPref(
      kBookmarksBubblePromoShownSyntheticTrialGroupNamePref, std::string());

#if BUILDFLAG(IS_ANDROID)
  // Deprecated 03/2026.
  registry->RegisterListPref(kPrivacySandboxActivityTypeRecord2);
#endif  // BUILDFLAG(IS_ANDROID)

  // Deprecated 03/2026.
  registry->RegisterIntegerPref(kTabOrganizationNudgeBackoffCount, 0);
  registry->RegisterBooleanPref(kTabOrganizationShowFRE, true);
  registry->RegisterIntegerPref(kTabOrganizationModelStrategy, 0);

  // Deprecated 03/2026.
  registry->RegisterIntegerPref(kNtpContextMenuClickCount, 0);

  // Deprecated 03/2026.
  registry->RegisterTimePref(kNtpPromoPrefLastSnoozed, base::Time());

  // Deprecated 03/2026.
  registry->RegisterIntegerPref(kSafeBrowsingModuleShownCount, 0);
  registry->RegisterInt64Pref(kSafeBrowsingModuleLastCooldownStartAt, 0);
  registry->RegisterBooleanPref(kSafeBrowsingModuleOpened, false);

  // Deprecated 04/2026.
  registry->RegisterIntegerPref(kPreallocatedAddressesVersion, 1);
  registry->RegisterListPref(kPreallocatedAddresses);
  registry->RegisterIntegerPref(kPreallocatedAddressesNext, 0);
  registry->RegisterTimePref(kFirstPlusAddressCreationTime, base::Time());
  registry->RegisterTimePref(kLastPlusAddressFillingTime, base::Time());

  // Deprecated 05/2026.
  registry->RegisterIntegerPref(kWebFeedContentOrder, 0);
  registry->RegisterDictionaryPref(kWebFeedsRequestSchedule);
  registry->RegisterBooleanPref(kEnableWebFeedFollowIntroDebug, false);
  registry->RegisterIntegerPref(kLastSeenFeedType, 0);

  // Deprecated 05/2026.
  registry->RegisterBooleanPref(kShouldShowRemoteAnnotatorFirstRunInfo, true);

  // Deprecated 05/2026.
  registry->RegisterIntegerPref(
      kContextualCueingEnterprisePolicyAllowedDeprecated, 0);

  // Deprecated 06/2026.
  registry->RegisterIntegerPref(kDeleteTimePeriodBasic, 0);
  registry->RegisterBooleanPref(kDeleteBrowsingHistoryBasic, true);
  registry->RegisterBooleanPref(kDeleteCacheBasic, true);
  registry->RegisterBooleanPref(kDeleteCookiesBasic, true);
  registry->RegisterIntegerPref(kLastClearBrowsingDataTab, 0);
  registry->RegisterIntegerPref(kGlicSelectionWidgetDismissCount, 0);

  // Deprecated 06/2026.
  registry->RegisterDictionaryPref(kRealboxContextMenuAnimationState);

  // Deprecated 06/2026.
  registry->RegisterBooleanPref(kTabSearchMigrationComplete, true);
  registry->RegisterBooleanPref(kTabSearchPinnedToTabstripMigrationComplete,
                                true);
  registry->RegisterBooleanPref(kTabSearchPinnedToTabstripMigrationComplete2,
                                true);

  // Deprecated 06/2026.
  registry->RegisterBooleanPref(kPersonalContextInAutofillNoticeShouldBeShown,
                                true);

  // Deprecated 06/2026.
  registry->RegisterInt64Pref(kDefaultBrowserInfobarLastDeclined, 0);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  // Deprecated 07/2026.
  registry->RegisterBooleanPref(kProxyOverrideRulesAffiliation, true);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Deprecated 07/2026.
  registry->RegisterBooleanPref(kMV2DeprecationWarningAcknowledgedGlobally,
                                false);
  registry->RegisterBooleanPref(kMV2DeprecationDisabledAcknowledgedGlobally,
                                false);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  // Deprecated 07/2026.
  registry->RegisterTimePref(kObsoleteManagementProfileLastLogTime,
                             base::Time());

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 07/2026.
  registry->RegisterBooleanPref(prefs::kProjectsPanelEntrypointEnabled, true);
  registry->RegisterBooleanPref(prefs::kProjectsPanelPinnedToTabstrip, true);
#endif

  // Deprecated 08/2026.
  registry->RegisterDictionaryPref(kPrivacySandboxNotices);
  registry->RegisterTimePref(kPrivacySandboxTopicsDataAccessibleSince,
                             base::Time());
  registry->RegisterListPref(kPrivacySandboxBlockedTopics);
  registry->RegisterDictionaryPref(kPrivacySandboxFledgeJoinBlocked);
  registry->RegisterBooleanPref(kPrivacySandboxM1ConsentDecisionMade, false);
  registry->RegisterBooleanPref(kPrivacySandboxM1EEANoticeAcknowledged, false);
  registry->RegisterBooleanPref(kPrivacySandboxM1RowNoticeAcknowledged, false);
  registry->RegisterBooleanPref(kPrivacySandboxM1RestrictedNoticeAcknowledged,
                                false);
  registry->RegisterIntegerPref(kPrivacySandboxM1PromptSuppressed, 0);
  registry->RegisterBooleanPref(kPrivacySandboxNoticeDisplayed, false);
  registry->RegisterBooleanPref(kPrivacySandboxConsentDecisionMade, false);
  registry->RegisterBooleanPref(kPrivacySandboxNoConfirmationSandboxDisabled,
                                false);
  registry->RegisterBooleanPref(kPrivacySandboxNoConfirmationSandboxRestricted,
                                false);
  registry->RegisterBooleanPref(kPrivacySandboxNoConfirmationSandboxManaged,
                                false);
  registry->RegisterBooleanPref(
      kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked, false);
  registry->RegisterBooleanPref(kPrivacySandboxNoConfirmationManuallyControlled,
                                false);
  registry->RegisterBooleanPref(kPrivacySandboxDisabledInsufficientConfirmation,
                                false);
  registry->RegisterBooleanPref(kPrivacySandboxTopicsConsentGiven, false);
  registry->RegisterTimePref(kPrivacySandboxTopicsConsentLastUpdateTime,
                             base::Time());
  registry->RegisterIntegerPref(kPrivacySandboxTopicsConsentLastUpdateReason,
                                0);
  registry->RegisterStringPref(kPrivacySandboxTopicsConsentTextAtLastUpdate,
                               "");
  registry->RegisterBooleanPref(kPrivacySandboxAllowNoticeFor3PCBlockedTrial,
                                false);

  // Deprecated 08/2026.
  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabled, true);
  registry->RegisterBooleanPref(kObsoleteAutofillWalletImportEnabledMigrated,
                                false);

  // Deprecated 08/2026.
  registry->RegisterBooleanPref(kShowRollbackUiModeB, false);
  registry->RegisterBooleanPref(kBlockAll3pcToggleEnabled, false);
  registry->RegisterBooleanPref(kTrackingProtection3pcdEnabled, false);
}

}  // namespace

std::string GetCountry() {
  if (!g_browser_process || !g_browser_process->variations_service()) {
    // This should only happen in tests. Ideally this would be guarded by
    // CHECK_IS_TEST, but that is not set on Android, so no specific guard.
    return std::string();
  }
  return std::string(
      g_browser_process->variations_service()->GetStoredPermanentCountry());
}

void RegisterLocalState(PrefRegistrySimple* registry) {
  // Call outs to individual subsystems that register Local State (browser-wide)
  // prefs en masse. See RegisterProfilePrefs for per-profile prefs. Please
  // keep this list alphabetized.
#if BUILDFLAG(IS_ANDROID)
  accessibility::AccessibilityPrefsController::RegisterLocalStatePrefs(
      registry);
#endif
  autofill::prefs::RegisterLocalStatePrefs(registry);
  breadcrumbs::RegisterPrefs(registry);
  browser_shutdown::RegisterPrefs(registry);
  BrowserProcessImpl::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterLocalStatePrefs(registry);
#if BUILDFLAG(CHROME_FOR_TESTING)
  chrome_for_testing::RegisterPrefs(registry);
#endif
  chrome_urls::RegisterPrefs(registry);
  ChromeMetricsServiceClient::RegisterPrefs(registry);
  enterprise_connectors::RegisterLocalStatePrefs(registry);
  enterprise_groups::RegisterLocalStatePrefs(registry);
  enterprise_util::RegisterLocalStatePrefs(registry);
  component_updater::RegisterPrefs(registry);
  domain_reliability::RegisterPrefs(registry);
  embedder_support::OriginTrialPrefs::RegisterPrefs(registry);
  enterprise_reporting::RegisterLocalStatePrefs(registry);
  ExternalProtocolHandler::RegisterPrefs(registry);
  flags_ui::PrefServiceFlagsStorage::RegisterPrefs(registry);
  GpuModeManager::RegisterPrefs(registry);
  signin::IdentityManager::RegisterLocalStatePrefs(registry);
  language::GeoLanguageProvider::RegisterLocalStatePrefs(registry);
  language::UlpLanguageCodeLocator::RegisterLocalStatePrefs(registry);
  memory::EnterpriseMemoryLimitPrefObserver::RegisterPrefs(registry);
  metrics::RegisterDemographicsLocalStatePrefs(registry);
  metrics::TabStatsTracker::RegisterPrefs(registry);
  network_time::NetworkTimeTracker::RegisterPrefs(registry);
  omnibox::RegisterLocalStatePrefs(registry);
#if !BUILDFLAG(IS_ANDROID)
  omnibox_everywhere::prefs::RegisterLocalStatePrefs(registry);
#endif
  optimization_guide::prefs::RegisterLocalStatePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterLocalStatePrefs(registry);
  password_manager::PasswordManager::RegisterLocalPrefs(registry);
  policy::BrowserPolicyConnector::RegisterPrefs(registry);
  policy::LocalTestPolicyProvider::RegisterLocalStatePrefs(registry);
  policy::ManagementService::RegisterLocalStatePrefs(registry);
  policy::PolicyStatisticsCollector::RegisterPrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterPrefs(registry);
  ProfileAttributesEntry::RegisterLocalStatePrefs(registry);
  ProfileAttributesStorage::RegisterPrefs(registry);
  ProfileNetworkContextService::RegisterLocalStatePrefs(registry);
  profiles::RegisterPrefs(registry);
  feature_engagement::RegisterLocalStatePrefs(registry);
#if BUILDFLAG(IS_ANDROID)
  PushMessagingServiceImpl::RegisterPrefs(registry);
#endif
  RegisterScreenshotPrefs(registry);
  safe_browsing::RegisterLocalStatePrefs(registry);
  search_engines::SearchEngineChoiceService::RegisterLocalStatePrefs(registry);
  secure_origin_allowlist::RegisterPrefs(registry);
  segmentation_platform::SegmentationPlatformService::RegisterLocalStatePrefs(
      registry);
  SerialPolicyAllowedPorts::RegisterPrefs(registry);
  HidPolicyAllowedDevices::RegisterLocalStatePrefs(registry);
  sessions::SessionIdGenerator::RegisterPrefs(registry);
  signin::ActivePrimaryAccountsMetricsRecorder::RegisterLocalStatePrefs(
      registry);
  SSLConfigServiceManager::RegisterPrefs(registry);
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(
      registry, subresource_filter::kSafeBrowsingRulesetConfig.filter_tag);
  SystemNetworkContextManager::RegisterPrefs(registry);
  tracing::RegisterPrefs(registry);
  update_client::RegisterPrefs(registry);
  variations::VariationsService::RegisterPrefs(registry);

  // Individual preferences. If you have multiple preferences that should
  // clearly be grouped together, please group them together into a helper
  // function called above. Please keep this list alphabetized.

  registry->RegisterTimePref(prefs::kAudioInputStreamLastTimeCreated,
                             base::Time(), PrefRegistry::LOSSY_PREF);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kIntensiveWakeUpThrottlingEnabled, false);
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(policy::policy_prefs::kBackForwardCacheEnabled,
                                true);
  registry->RegisterBooleanPref(policy::policy_prefs::kReadAloudEnabled, true);
#endif  // BUILDFLAG(IS_ANDROID)

  // Below this point is for platform-specific and compile-time conditional
  // calls. Please follow the helper-function-first-then-direct-calls pattern
  // established above, and keep things alphabetized.

#if BUILDFLAG(ENABLE_BACKGROUND_MODE)
  BackgroundModeManager::RegisterPrefs(registry);
#endif

#if BUILDFLAG(IS_ANDROID)
  ::android::RegisterPrefs(registry);

  registry->RegisterIntegerPref(first_run::kTosDialogBehavior, 0);
  registry->RegisterBooleanPref(lens::kLensCameraAssistedSearchEnabled, true);
#else   // BUILDFLAG(IS_ANDROID)
  headless::RegisterPrefs(registry);
  IntranetRedirectDetector::RegisterPrefs(registry);
  media_router::RegisterLocalStatePrefs(registry);
  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(registry);
  PerformanceInterventionMetricsReporter::RegisterLocalStatePrefs(registry);
  RegisterBrowserPrefs(registry);
  speech::SodaInstaller::RegisterLocalStatePrefs(registry);
  StartupBrowserCreator::RegisterLocalStatePrefs(registry);
  task_manager::TaskManagerInterface::RegisterPrefs(registry);
  UpgradeDetector::RegisterPrefs(registry);
  registry->RegisterIntegerPref(prefs::kLastWhatsNewVersion, 0);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ExtensionPrefs::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  on_device_translation::RegisterLocalStatePrefs(registry);
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  WhatsNewUI::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  FirstRunService::RegisterLocalStatePrefs(registry);
#endif

#if BUILDFLAG(IS_MAC)
  confirm_quit::RegisterLocalState(registry);
  QuitWithAppsController::RegisterPrefs(registry);
  system_media_permissions::RegisterSystemMediaPermissionStatesPrefs(registry);
  AppShimRegistry::Get()->RegisterLocalPrefs(registry);
  GlassFrameService::RegisterLocalStatePrefs(registry);

  // The default value is not signicant as this preference is only consulted if
  // it is explicitly set by an enterprise policy.
  registry->RegisterBooleanPref(prefs::kWebAppsUseAdHocCodeSigningForAppShims,
                                false);
  registry->RegisterBooleanPref(prefs::kUpdateOnZeroWindowEnabled, true);
#endif

#if BUILDFLAG(ENABLE_DOWNGRADE_PROCESSING)
  downgrade::RegisterPrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID)
  RegisterDefaultBrowserPromptPrefs(registry);
  DeviceOAuth2TokenStoreDesktop::RegisterPrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID)
  screen_ai::RegisterLocalStatePrefs(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  PlatformAuthPolicyObserver::RegisterPrefs(registry);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

  // Platform-specific and compile-time conditional individual preferences.
  // If you have multiple preferences that should clearly be grouped together,
  // please group them together into a helper function called above. Please
  // keep this list alphabetized.

#if BUILDFLAG(ENABLE_OOP_PRINTING)
  registry->RegisterBooleanPref(prefs::kOopPrintDriversAllowedByPolicy, true);
#endif

#if BUILDFLAG(ENABLE_PDF)
  registry->RegisterBooleanPref(prefs::kPdfViewerOutOfProcessIframeEnabled,
                                true);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  registry->RegisterStringPref(
      prefs::kRestrictPdfSaveToGoogleDriveAccountsToPattern, "");
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kChromeForTestingAllowed, true);
#endif

  registry->RegisterBooleanPref(prefs::kQRCodeGeneratorEnabled, true);

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

  glic::prefs::RegisterLocalStatePrefs(registry);

  registry->RegisterIntegerPref(prefs::kToastAlertLevel, 0);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterStringPref(prefs::kNonMilestoneUpdateToastVersion, "");
  registry->RegisterBooleanPref(prefs::kSilentPrintingEnabled, false);
#endif  // !BUILDFLAG(IS_ANDROID)

  registry->RegisterListPref(
      prefs::kManagedLocalNetworkAccessIpAddressSpaceOverrides);
  registry->RegisterBooleanPref(
      policy::policy_prefs::kLocalNetworkAccessPermissionsPolicyDefaultEnabled,
      false);

  // This is intentionally last.
  RegisterLocalStatePrefsForMigration(registry);
}

// Register prefs applicable to all profiles.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                          const std::string& locale) {
  TRACE_EVENT0("browser", "chrome::RegisterProfilePrefs");
  // User prefs. Please keep this list alphabetized.
  AccessibilityLabelsService::RegisterProfilePrefs(registry);
  AccessibilityUIMessageHandler::RegisterProfilePrefs(registry);
  accessibility_annotator::prefs::RegisterProfilePrefs(registry);
  AimEligibilityService::RegisterProfilePrefs(registry);
  AnnouncementNotificationService::RegisterProfilePrefs(registry);
  autofill::prefs::RegisterProfilePrefs(registry);
  browsing_data::prefs::RegisterBrowserUserPrefs(registry);
  capture_policy::RegisterProfilePrefs(registry);
  certificate_transparency::prefs::RegisterPrefs(registry);
  ChromeContentBrowserClient::RegisterProfilePrefs(registry);
  ChromeLocationBarModelDelegate::RegisterProfilePrefs(registry);
  content_settings::CookieSettings::RegisterProfilePrefs(registry);
  ChromeVersionService::RegisterProfilePrefs(registry);
  chrome_browser_net::NetErrorTabHelper::RegisterProfilePrefs(registry);
  chrome_prefs::RegisterProfilePrefs(registry);
  collaboration::prefs::RegisterProfilePrefs(registry);
  commerce::RegisterProfilePrefs(registry);
  context_hub::prefs::RegisterProfilePrefs(registry);
  contextual_cueing::prefs::RegisterProfilePrefs(registry);
  contextual_search::ContextualSearchService::RegisterProfilePrefs(registry);
  contextual_tasks::RegisterProfilePrefs(registry);
  registry->RegisterIntegerPref(prefs::kContextualTasksNextPanelOpenCount, 0);
  registry->RegisterBooleanPref(prefs::kCtrlTabMru, false);
  cross_device::RegisterProfilePrefs(registry);
  enterprise::RegisterIdentifiersProfilePrefs(registry);
  enterprise_connectors::RegisterProfilePrefs(registry);
  enterprise_isolated_mode::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENTERPRISE_PROXY)
  enterprise_net::RegisterProfilePrefs(registry);
#endif
  enterprise_promotion::RegisterProfilePrefs(registry);
  enterprise_reporting::RegisterProfilePrefs(registry);
  dom_distiller::DistilledPagePrefs::RegisterProfilePrefs(registry);
  DownloadPrefs::RegisterProfilePrefs(registry);
  finds::FindsService::RegisterProfilePrefs(registry);
  glic::prefs::RegisterProfilePrefs(registry);
  glic::contextual_cueing::prefs::RegisterProfilePrefs(registry);
  permissions::PermissionHatsTriggerHelper::RegisterProfilePrefs(registry);
  history_clusters::prefs::RegisterProfilePrefs(registry);
  HostContentSettingsMap::RegisterProfilePrefs(registry);
  image_fetcher::ImageCache::RegisterProfilePrefs(registry);
  site_engagement::ImportantSitesUtil::RegisterProfilePrefs(registry);
  IncognitoModePrefs::RegisterProfilePrefs(registry);
  language::LanguagePrefs::RegisterProfilePrefs(registry);
  login_detection::prefs::RegisterProfilePrefs(registry);
  lookalikes::RegisterProfilePrefs(registry);
  media_prefs::RegisterUserPrefs(registry);
  MediaCaptureDevicesDispatcher::RegisterProfilePrefs(registry);
  media_device_salt::MediaDeviceIDSalt::RegisterProfilePrefs(registry);
  MediaEngagementService::RegisterProfilePrefs(registry);
  MediaStorageIdSalt::RegisterProfilePrefs(registry);
  metrics::RegisterDemographicsProfilePrefs(registry);
  metrics::MetricsReportingChoiceService::RegisterProfilePrefs(registry);
  NotificationDisplayServiceImpl::RegisterProfilePrefs(registry);
  NotifierStateTracker::RegisterProfilePrefs(registry);
  ntp_tiles::CustomLinksManagerImpl::RegisterProfilePrefs(registry);
  ntp_tiles::MostVisitedSites::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  on_device_translation::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  optimization_guide::prefs::RegisterProfilePrefs(registry);
  optimization_guide::model_execution::prefs::RegisterProfilePrefs(registry);
#if !BUILDFLAG(IS_ANDROID)
  PageColorsController::RegisterProfilePrefs(registry);
#endif
  password_manager::PasswordManager::RegisterProfilePrefs(registry);
  payments::RegisterProfilePrefs(registry);
  performance_manager::user_tuning::prefs::RegisterProfilePrefs(registry);
  permissions::RegisterProfilePrefs(registry);
  PermissionBubbleMediaAccessHandler::RegisterProfilePrefs(registry);
  PlatformNotificationServiceImpl::RegisterProfilePrefs(registry);
  policy::URLBlocklistManager::RegisterProfilePrefs(registry);
  PolicyUI::RegisterProfilePrefs(registry);
  PrefProxyConfigTrackerImpl::RegisterProfilePrefs(registry);
  prefetch::RegisterPredictionOptionsProfilePrefs(registry);
  PrefetchOriginDecider::RegisterPrefs(registry);
  PrefsTabHelper::RegisterProfilePrefs(registry, locale);
  personal_context::prefs::RegisterProfilePrefs(registry);
  privacy_sandbox::RegisterProfilePrefs(registry);
  Profile::RegisterProfilePrefs(registry);
  ProfileImpl::RegisterProfilePrefs(registry);
  ProfileNetworkContextService::RegisterProfilePrefs(registry);
  custom_handlers::ProtocolHandlerRegistry::RegisterProfilePrefs(registry);
  QuietNotificationPermissionUiState::RegisterProfilePrefs(registry);
  regional_capabilities::prefs::RegisterProfilePrefs(registry);
  RegisterBrowserUserPrefs(registry);
  registry->RegisterBooleanPref(prefs::kRestrictYouTubeCookiesDeletion, false);
  RegisterGeminiSettingsPrefs(registry);
  registry->RegisterIntegerPref(prefs::kVoiceTypingSettings, 0);
  registry->RegisterBooleanPref(prefs::kPrefDictationOnboardingCompleted,
                                false);
#if BUILDFLAG(IS_LINUX)
  registry->RegisterStringPref(prefs::kVoiceTypingHotkey, "Ctrl+Space");
#else
  registry->RegisterStringPref(prefs::kVoiceTypingHotkey, "Alt+Space");
#endif

#if !BUILDFLAG(IS_ANDROID)
  indigo::prefs::RegisterProfilePrefs(registry);
#endif
  RegisterPrefersDefaultScrollbarStylesPrefs(registry);
#if BUILDFLAG(IS_ANDROID)
  RegisterSafetyHubProfilePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  safe_browsing::file_type::RegisterProfilePrefs(registry);
#endif
  safe_browsing::RegisterProfilePrefs(registry);
  safety_check::prefs::RegisterProfilePrefs(registry);
  SearchPrefetchService::RegisterProfilePrefs(registry);
  blocked_content::SafeBrowsingTriggeredPopupBlocker::RegisterProfilePrefs(
      registry);
  security_interstitials::InsecureFormBlockingPage::RegisterProfilePrefs(
      registry);
  segmentation_platform::SegmentationPlatformService::RegisterProfilePrefs(
      registry);
  segmentation_platform::DeviceSwitcherResultDispatcher::RegisterProfilePrefs(
      registry);
  SearchPreloadService::RegisterProfilePrefs(registry);
  SessionStartupPref::RegisterProfilePrefs(registry);
  SigninPrefs::RegisterProfilePrefs(registry);
  site_engagement::SiteEngagementService::RegisterProfilePrefs(registry);
  skills::prefs::RegisterProfilePrefs(registry);
  subscription_eligibility::prefs::RegisterProfilePrefs(registry);
  supervised_user::RegisterProfilePrefs(registry);
  sync_sessions::SessionSyncPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceInfoPrefs::RegisterProfilePrefs(registry);
  syncer::DeviceStatisticsScheduler::RegisterProfilePrefs(registry);
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  syncer::SyncTransportDataPrefs::RegisterProfilePrefs(registry);
  TemplateURLPrepopulateData::RegisterProfilePrefs(registry);
  tab_groups::prefs::RegisterProfilePrefs(registry);
  translate::TranslatePrefs::RegisterProfilePrefs(registry);
  universal_optout::prefs::RegisterProfilePrefs(registry);
  visited_url_ranking::GroupSuggestionsServiceImpl::RegisterProfilePrefs(
      registry);
  wallet::prefs::RegisterProfilePrefs(registry);
  omnibox::RegisterProfilePrefs(registry);
  ZeroSuggestProvider::RegisterProfilePrefs(registry);
  NtpCustomBackgroundService::RegisterProfilePrefs(registry);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  RegisterSessionServiceLogProfilePrefs(registry);
  SessionDataService::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  extensions::ActivityLog::RegisterProfilePrefs(registry);
  extensions::PermissionsManager::RegisterProfilePrefs(registry);
  extensions::ExtensionPrefs::RegisterProfilePrefs(registry);
  extensions::RuntimeAPI::RegisterPrefs(registry);
  extensions::CommandService::RegisterProfilePrefs(registry);
  extensions::util::RegisterProfilePrefs(registry);
  extensions_ui_prefs::RegisterProfilePrefs(registry);
  ExtensionUrlOverrides::RegisterProfilePrefs(registry);
  update_client::RegisterProfilePrefs(registry);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(prefs::kPinExtensionsMenuButton, true);
#endif

#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  RegisterAnimationPolicyPrefs(registry);
  extensions::AudioAPI::RegisterUserPrefs(registry);
  // TODO(devlin): This would be more inline with the other calls here if it
  // were nested in either a class or separate namespace with a simple
  // Register[Profile]Prefs() name.
  extensions::RegisterSettingsOverriddenUiPrefs(registry);
  ExtensionSettingsOverriddenDialog::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(ENABLE_PDF)
  registry->RegisterListPref(prefs::kPdfLocalFileAccessAllowedForDomains,
                             base::ListValue());
  registry->RegisterBooleanPref(prefs::kPdfUseSkiaRendererEnabled, true);
#endif  // BUILDFLAG(ENABLE_PDF)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  printing::PolicySettings::RegisterProfilePrefs(registry);
  printing::PrintPreviewStickySettings::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_RLZ)
  ChromeRLZTrackerDelegate::RegisterProfilePrefs(registry);
#endif

  UnifiedAutoplayConfig::RegisterProfilePrefs(registry);
#if BUILDFLAG(ENABLE_WEBUI_NTP)
// TODO(b/502297163): Implement for Android.
#if !BUILDFLAG(IS_ANDROID)
  NewTabFooterUI::RegisterProfilePrefs(registry);
#endif  // !BUILDFLAG(IS_ANDROID)
  NewTabPageHandler::RegisterProfilePrefs(registry);
  NewTabPageUI::RegisterProfilePrefs(registry);
  MostVisitedPrefObserver::RegisterProfilePrefs(registry);
  MostRelevantTabResumptionPageHandler::RegisterProfilePrefs(registry);
  DriveService::RegisterProfilePrefs(registry);
  GoogleCalendarPageHandler::RegisterProfilePrefs(registry);
#else
  // Registered here because it is still accessed on Android (which doesn't use
  // WebUI NTP).
  registry->RegisterBooleanPref(ntp_prefs::kNtpShortcutsVisible, true);
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(IS_ANDROID)
  AuxiliarySearchDonationService::RegisterProfilePrefs(registry);
  feed::prefs::RegisterFeedSharedProfilePrefs(registry);
  feed::RegisterProfilePrefs(registry);
  cdm::MediaDrmStorageImpl::RegisterProfilePrefs(registry);
  KnownInterceptionDisclosureInfoBarDelegate::RegisterProfilePrefs(registry);
  MediaDrmOriginIdManager::RegisterProfilePrefs(registry);
  NotificationChannelsProviderAndroid::RegisterProfilePrefs(registry);
  NtpAndroidCustomBackgroundService::RegisterProfilePrefs(registry);
  ntp_tiles::PopularSitesImpl::RegisterProfilePrefs(registry);
  OomInterventionDecider::RegisterProfilePrefs(registry);
  PartnerBookmarksShim::RegisterProfilePrefs(registry);
  permissions::GeolocationPermissionContextAndroid::RegisterProfilePrefs(
      registry);
  readaloud::RegisterProfilePrefs(registry);
  RecentTabsPagePrefs::RegisterProfilePrefs(registry);
  usage_stats::UsageStatsBridge::RegisterProfilePrefs(registry);
  variations::VariationsService::RegisterProfilePrefs(registry);
  webapps::InstallPromptPrefs::RegisterProfilePrefs(registry);
#else   // BUILDFLAG(IS_ANDROID)
  bookmarks_webui::RegisterProfilePrefs(registry);
  browser_sync::ForeignSessionHandler::RegisterProfilePrefs(registry);
  BrowserUserEducationStorageService::RegisterProfilePrefs(registry);
  captions::LiveCaptionController::RegisterProfilePrefs(registry);
  captions::LiveTranslateController::RegisterProfilePrefs(registry);
  ChromeAuthenticatorRequestDelegate::RegisterProfilePrefs(registry);
  commerce::CommerceUiTabHelper::RegisterProfilePrefs(registry);
  first_run::RegisterProfilePrefs(registry);
  HatsServiceDesktop::RegisterProfilePrefs(registry);
  lens::prefs::RegisterProfilePrefs(registry);
  media_router::RegisterAccessCodeProfilePrefs(registry);
  media_router::RegisterProfilePrefs(registry);
  MicrosoftAuthPageHandler::RegisterProfilePrefs(registry);
  MicrosoftFilesPageHandler::RegisterProfilePrefs(registry);
  OutlookCalendarPageHandler::RegisterProfilePrefs(registry);
  PinnedTabCodec::RegisterProfilePrefs(registry);
  promos_utils::RegisterProfilePrefs(registry);
  PromoService::RegisterProfilePrefs(registry);
  settings::SettingsUI::RegisterProfilePrefs(registry);
  send_tab_to_self::RegisterProfilePrefs(registry);
  signin::RegisterProfilePrefs(registry);
  StartupBrowserCreator::RegisterProfilePrefs(registry);
  TabGroupsPageHandler::RegisterProfilePrefs(registry);
  tab_groups::saved_tab_groups::prefs::RegisterProfilePrefs(registry);
  tab_search_prefs::RegisterProfilePrefs(registry);
  ThemeColorPickerHandler::RegisterProfilePrefs(registry);
  ThemeService::RegisterProfilePrefs(registry);
  toolbar::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  ManagementUI::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)
  DevToolsWindow::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENABLE_DEVTOOLS_FRONTEND)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  device_signals::RegisterProfilePrefs(registry);
  ntp_tiles::EnterpriseShortcutsManagerImpl::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  browser_switcher::BrowserSwitcherPrefs::RegisterProfilePrefs(registry);
  enterprise_signin::RegisterProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  preinstalled_extensions::RegisterProfilePrefs(registry);
#endif

#if !BUILDFLAG(IS_ANDROID)
  sharing_hub::RegisterProfilePrefs(registry);
#endif

#if defined(TOOLKIT_VIEWS)
  accessibility_prefs::RegisterInvertBubbleUserPrefs(registry);
  side_search_prefs::RegisterProfilePrefs(registry);
  RegisterBrowserViewProfilePrefs(registry);
#endif

#if BUILDFLAG(ENABLE_LENS_DESKTOP)
  registry->RegisterBooleanPref(
      prefs::kLensRegionSearchEnabled, true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif
#if BUILDFLAG(ENABLE_LENS_DESKTOP) || BUILDFLAG(ENABLE_WEBUI_NTP)
  registry->RegisterBooleanPref(prefs::kLensDesktopNTPSearchEnabled, true);
#endif

  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
  registry->RegisterDictionaryPref(prefs::kContextMenuAnimationState);
  registry->RegisterListPref(
      webauthn::pref_names::kRemoteDesktopAllowedOrigins);

  enterprise_custom_headers::RegisterProfilePrefs(registry);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      webauthn::pref_names::kRemoteProxiedRequestsAllowed, false);

  registry->RegisterIntegerPref(
      webauthn::pref_names::kEnclaveFailedPINAttemptsCount, 0);

  side_panel_prefs::RegisterProfilePrefs(registry);

  tabs::RegisterProfilePrefs(registry);

  CertificateManagerPageHandler::RegisterProfilePrefs(registry);

  actor::ui::RegisterProfilePrefs(registry);

  organizer::RegisterProfilePrefs(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  registry->RegisterBooleanPref(webauthn::pref_names::kAllowWithBrokenCerts,
                                false);

  registry->RegisterBooleanPref(prefs::kPrivacyGuideViewed, false);

  RegisterProfilePrefsForMigration(registry);

#if !BUILDFLAG(IS_ANDROID)
  registry->RegisterIntegerPref(prefs::kMemorySaverChipExpandedCount, 0);
  registry->RegisterTimePref(prefs::kLastMemorySaverChipExpandedTimestamp,
                             base::Time());
  registry->RegisterBooleanPref(
      prefs::kAccessibilityMainNodeAnnotationsEnabled, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#endif  // !BUILDFLAG(IS_ANDROID)

  registry->RegisterBooleanPref(
      prefs::kManagedLocalNetworkAccessRestrictionsTemporaryOptOut, false);

#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kAppRatingPromptShown, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kVirtualKeyboardResizesLayoutByDefault,
                                false);
#endif

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  data_controls::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

#if BUILDFLAG(ENABLE_COMPOSE)
  registry->RegisterBooleanPref(prefs::kPrefHasCompletedComposeFRE, false);
  registry->RegisterBooleanPref(prefs::kEnableProactiveNudge, true);
  registry->RegisterDictionaryPref(prefs::kProactiveNudgeDisabledSitesWithTime);
#endif

  registry->RegisterIntegerPref(prefs::kChromeDataRegionSetting, 0);

  registry->RegisterIntegerPref(prefs::kLensOverlayStartCount, 0);

  registry->RegisterBooleanPref(prefs::kViewSourceLineWrappingEnabled, false);

  // TODO(crbug.com/442891187): Move these to appropriate manager files when
  // the policies logic is implemented.
  registry->RegisterListPref(policy::policy_prefs::kIncognitoModeUrlBlocklist);
  registry->RegisterListPref(policy::policy_prefs::kIncognitoModeUrlAllowlist);

  registry->RegisterBooleanPref(
      ntp_tiles::prefs::kTabResumptionHomeModuleEnabled, true);

  registry->RegisterBooleanPref(ntp_tiles::prefs::kMagicStackHomeModuleEnabled,
                                true);

  registry->RegisterBooleanPref(ntp_tiles::prefs::kTipsHomeModuleEnabled, true);

#if BUILDFLAG(IS_ANDROID)
  tips::prefs::RegisterProfilePrefs(registry);
#endif  // BUILDFLAG(IS_ANDROID)

  registry->RegisterBooleanPref(prefs::kStaticStorageQuotaEnabled, false);
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  RegisterUserProfilePrefs(registry, g_browser_process->GetApplicationLocale());
}

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry,
                              const std::string& locale) {
  RegisterProfilePrefs(registry, locale);

#if BUILDFLAG(IS_ANDROID)
  ::android::RegisterUserProfilePrefs(registry);
#endif
}

void RegisterScreenshotPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDisableScreenshots, false);
}

void RegisterGeminiSettingsPrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(optimization_guide::prefs::kGeminiSettings, 0);
}

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteLocalStatePrefs(PrefService* local_state) {
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

  // Added 08/2025.
  local_state->ClearPref(kInvalidationClientIDCache);
  local_state->ClearPref(kInvalidationTopicsToHandler);

  // Added 09/2025
  local_state->ClearPref(kRendererCodeIntegrityEnabledNeedsDeletion);

  // Added 10/2025
#if !BUILDFLAG(IS_ANDROID)
  local_state->ClearPref(prefs::kDefaultBrowserFirstShownTime);
#endif

  // Added 11/2025
  local_state->ClearPref(kFpfRulesetContent);
  local_state->ClearPref(kFpfRulesetFormat);
  local_state->ClearPref(kFpfRulesetChecksum);

  // Added 12/2025
  local_state->ClearPref(kPrivacyBudgetGeneration);
  local_state->ClearPref(kPrivacyBudgetSeenSurfaces);
  local_state->ClearPref(kPrivacyBudgetSelectedOffsets);
  local_state->ClearPref(kPrivacyBudgetSelectedBlock);
  local_state->ClearPref(kPrivacyBudgetMetaExperimentActivationSalt);

  // Added 12/2025
  local_state->ClearPref(kAutofillStatesDataDir);

  // Added 12/2025
  local_state->ClearPref(kFingerprintingProtectionEnabled);
  local_state->ClearPref(kIpProtectionEnabled);
  local_state->ClearPref(kAllowAll3pcToggleEnabled);
  local_state->ClearPref(kUserBypass3pcExceptionsMigrated);
  local_state->ClearPref(kTrackingProtectionLevel);
  local_state->ClearPref(kTrackingProtectionSilentOnboardedSince);
  local_state->ClearPref(kTrackingProtectionSilentEligibleSince);
  local_state->ClearPref(kTrackingProtectionEligibleSince);
  local_state->ClearPref(kTrackingProtectionOnboardedSince);
  local_state->ClearPref(kTrackingProtectionNoticeLastShown);
  local_state->ClearPref(kTrackingProtectionOnboardingAckedSince);
  local_state->ClearPref(kTrackingProtectionOnboardingAcked);
  local_state->ClearPref(kTrackingProtectionOnboardingAckAction);
  local_state->ClearPref(kIpProtectionInitializedByDogfood);
  local_state->ClearPref(kTrackingProtectionSilentOnboardingStatus);
  local_state->ClearPref(kTrackingProtectionOnboardingStatus);
  local_state->ClearPref(kTPCDExperimentClientState);
  local_state->ClearPref(kTPCDExperimentClientStateVersion);
  local_state->ClearPref(kTPCDExperimentProfileState);

  // Added 02/2026.
  if (local_state->HasPrefPath(kProfilesDeletedOld)) {
    const base::ListValue& old_list = local_state->GetList(kProfilesDeletedOld);
    if (!old_list.empty()) {
      ScopedListPrefUpdate update(local_state, prefs::kProfilesDeleted);
      for (const auto& value : old_list) {
        std::optional<base::FilePath> path = base::ValueToFilePath(value);
        if (path) {
          base::FilePath basename = path->BaseName();
          // Avoid the edge case where the base name is the root, e.g `/` on
          // linux.
          if (!basename.IsAbsolute()) {
            update->Append(base::FilePathToValue(basename));
          }
        }
      }
    }
    local_state->ClearPref(kProfilesDeletedOld);
  }

  // Added 03/2026.
  local_state->ClearPref(kGlicMultiInstanceEnabledBySubscriptionTier);

  // Added 04/2026.
  local_state->ClearPref(kTpcdMetadataCohorts);

#if !BUILDFLAG(IS_ANDROID)
  // Added 04/2026.
  tabs::MigrateHoverCardMemoryPref(local_state);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 04/2026.
  local_state->ClearPref(kHasSeenWebFeed);
  local_state->ClearPref(kLastBadgeAnimationTime);

  // Added 05/2026
  local_state->ClearPref(kHttpCacheFinchExperimentGroups);

  // Added 07/2026.
  local_state->ClearPref(kObsoleteMetricsReportingLevel);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  // Added 07/2026.
  local_state->ClearPref(kProxyOverrideRulesAffiliation);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  local_state->ClearPref(kMetricsReportingMigrationDone);
  local_state->ClearPref(kMetricsConsentRestructureFeatureState);

  // Added 07/2026.
  local_state->ClearPref(kObsoleteManagementPlatformLastLogTime);

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_LOCAL_STATE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}

// This method should be periodically pruned of year+ old migrations.
// See chrome/browser/prefs/README.md for details.
void MigrateObsoleteProfilePrefs(PrefService* profile_prefs,
                                 const base::FilePath& profile_path) {
  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.

  // BEGIN_MIGRATE_OBSOLETE_PROFILE_PREFS
  // Please don't delete the preceding line. It is used by PRESUBMIT.py.

#if !BUILDFLAG(IS_ANDROID)
  // Added 08/2024, but DO NOT REMOVE after the usual year.
  // TODO(crbug.com/356148174): Remove once kMoveThemePrefsToSpecifics has been
  // enabled for an year.
  MigrateSyncingThemePrefsToNonSyncingIfNeeded(profile_prefs);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Added 10/2025.
  profile_prefs->ClearPref(kSessionRestoreTurnOffFromRestartInfoBarTimesShown);
  profile_prefs->ClearPref(kSessionRestoreTurnOffFromSessionInfoBarTimesShown);
  profile_prefs->ClearPref(kSessionRestorePrefChanged);

  // Check MigrateDeprecatedAutofillPrefs() to see if this is safe to remove.
  autofill::prefs::MigrateDeprecatedAutofillPrefs(profile_prefs);

  // Added 08/2025.
  profile_prefs->ClearPref(kInvalidationClientIDCache);
  profile_prefs->ClearPref(kInvalidationTopicsToHandler);

#if BUILDFLAG(IS_ANDROID)
  // Added 08/2025.
  profile_prefs->ClearPref(kObsoleteAccountStorageNoticeShown);
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Deprecated 08/2025.
  profile_prefs->ClearPref(
      kObsoleteAutofillableCredentialsProfileStoreLoginDatabase);
  profile_prefs->ClearPref(
      kObsoleteAutofillableCredentialsAccountStoreLoginDatabase);
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  // Added 08/2025.
  MostVisitedPrefObserver::MigrateDeprecatedUseMostVisitedTilesPref(
      profile_prefs);
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(IS_ANDROID)
  // Added 09/2025.
  profile_prefs->ClearPref(kObsoleteUpmUnmigratedPasswordsExported);
  profile_prefs->ClearPref(kObsoletePasswordsUseUPMLocalAndSeparateStores);
  profile_prefs->ClearPref(kObsoleteEmptyProfileStoreLoginDatabase);
  profile_prefs->ClearPref(kObsoleteUpmAutoExportCsvNeedsDeletion);
  base::DeleteFile(profile_path.Append(FILE_PATH_LITERAL("Login Data")));
  base::DeleteFile(
      profile_path.Append(FILE_PATH_LITERAL("Login Data For Account")));
  base::DeleteFile(
      profile_path.Append(FILE_PATH_LITERAL("Login Data-journal")));
  base::DeleteFile(
      profile_path.Append(FILE_PATH_LITERAL("Login Data For Account-journal")));
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 09/2025.
#if !BUILDFLAG(IS_ANDROID)
  PageColorsController::MigrateObsoleteProfilePrefs(profile_prefs);
#endif
  profile_prefs->ClearPref(kGaiaCookieLastListAccountsData);

  // Added 09/2025.
  profile_prefs->ClearPref(kLensOverlayEduActionChipShownCount);

  SigninPrefs(*profile_prefs).MigrateObsoleteSigninPrefs();

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  // Added 10/2025
  MostVisitedPrefObserver::MigrateDeprecatedShortcutsTypePref(profile_prefs);
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

  // Added 10/2025.
  profile_prefs->ClearPref(kLegacySyncSessionsGUID);

  // Added 11/2025.
  profile_prefs->ClearPref(kRefreshHeuristicBreakageException);

  // Added 12/2025.
  profile_prefs->ClearPref(kReduceUserAgentMinorVersion);
  profile_prefs->ClearPref(kMerchantTrustUiLastInteractionTime);
  profile_prefs->ClearPref(kMerchantTrustPageInfoLastOpenTime);

  // Added 12/2025.
  profile_prefs->ClearPref(kCloudPrintProxyEnabled);
  profile_prefs->ClearPref(kCloudPrintEmail);

#if BUILDFLAG(IS_ANDROID)
  // Added 01/2026.
  profile_prefs->ClearPref(kDSEGeolocationSettingDeprecated);
  profile_prefs->ClearPref(kDSEPermissionsSettings);
  profile_prefs->ClearPref(kDSEWasDisabledByPolicy);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 01/2026.
  profile_prefs->ClearPref(kCookieClearOnExitMigrationNoticeComplete);

  // Added 02/2026.
  profile_prefs->ClearPref(kGlicGuestUrlPresetAutopush);
  profile_prefs->ClearPref(kGlicGuestUrlPresetPreprod);
  profile_prefs->ClearPref(kGlicGuestUrlPresetProd);

  // Added 02/2026.
  profile_prefs->ClearPref(kExplicitBrowserSigninWithoutFeatureEnabled);

  // Added 02/2026.
  profile_prefs->ClearPref(kTabSearchOpened);

  // Added 03/2026.
  profile_prefs->ClearPref(kTabDeclutterUsageCount);

  // Added 03/2026.
  profile_prefs->ClearPref(kTabSearchTabIndex);

  // Added 03/2026
  profile_prefs->ClearPref(
      kSigninFromBookmarksBubbleSyntheticTrialGroupNamePref);
  profile_prefs->ClearPref(
      kBookmarksBubblePromoShownSyntheticTrialGroupNamePref);

  // Added 03/2026.
  profile_prefs->ClearPref(kSafeBrowsingModuleShownCount);
  profile_prefs->ClearPref(kSafeBrowsingModuleLastCooldownStartAt);
  profile_prefs->ClearPref(kSafeBrowsingModuleOpened);

#if BUILDFLAG(IS_ANDROID)
  // Added 03/2026.
  profile_prefs->ClearPref(kPrivacySandboxActivityTypeRecord2);
#endif  // BUILDFLAG(IS_ANDROID)

  // Added 03/2026.
  profile_prefs->ClearPref(kTabOrganizationNudgeBackoffCount);
  profile_prefs->ClearPref(kTabOrganizationShowFRE);
  profile_prefs->ClearPref(kTabOrganizationModelStrategy);

  // Added 03/2026.
  profile_prefs->ClearPref(kNtpContextMenuClickCount);

  // Added 03/2026.
  profile_prefs->ClearPref(kNtpPromoPrefLastSnoozed);

  // Added 04/2026.
  profile_prefs->ClearPref(kPreallocatedAddressesVersion);
  profile_prefs->ClearPref(kPreallocatedAddresses);
  profile_prefs->ClearPref(kPreallocatedAddressesNext);
  profile_prefs->ClearPref(kFirstPlusAddressCreationTime);
  profile_prefs->ClearPref(kLastPlusAddressFillingTime);

  // Added 05/2026.
  profile_prefs->ClearPref(kWebFeedContentOrder);
  profile_prefs->ClearPref(kWebFeedsRequestSchedule);
  profile_prefs->ClearPref(kEnableWebFeedFollowIntroDebug);
  profile_prefs->ClearPref(kLastSeenFeedType);

  // Added 05/2026.
  profile_prefs->ClearPref(kShouldShowRemoteAnnotatorFirstRunInfo);

  // Deprecated 06/2026.
  profile_prefs->ClearPref(kDeleteTimePeriodBasic);
  profile_prefs->ClearPref(kDeleteBrowsingHistoryBasic);
  profile_prefs->ClearPref(kDeleteCacheBasic);
  profile_prefs->ClearPref(kDeleteCookiesBasic);
  profile_prefs->ClearPref(kLastClearBrowsingDataTab);

  // Added 06/2026.
  profile_prefs->ClearPref(kGlicSelectionWidgetDismissCount);

  // Added 06/2026.
  profile_prefs->ClearPref(kRealboxContextMenuAnimationState);

  // Added 06/2026
  profile_prefs->ClearPref(kTabSearchMigrationComplete);
  profile_prefs->ClearPref(kTabSearchPinnedToTabstripMigrationComplete);
  profile_prefs->ClearPref(kTabSearchPinnedToTabstripMigrationComplete2);

  // Added 06/2026.
  profile_prefs->ClearPref(kPersonalContextInAutofillNoticeShouldBeShown);

  // Added 06/2026
#if !BUILDFLAG(IS_ANDROID)
  profile_prefs->ClearPref(kDefaultBrowserInfobarLastDeclined);
#endif

  // Added 06/2026.
  syncer::ClearAccountKeyedPrefValue(
      profile_prefs, autofill::prefs::kAutofillAiOptInStatus, {});

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
  // Added 07/2026.
  profile_prefs->ClearPref(kProxyOverrideRulesAffiliation);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  // Added 07/2026.
  profile_prefs->ClearPref(kMV2DeprecationWarningAcknowledgedGlobally);
  profile_prefs->ClearPref(kMV2DeprecationDisabledAcknowledgedGlobally);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if !BUILDFLAG(IS_ANDROID)
  // Added 07/2026.
  tabs::MigrateEverythingMenuPinnedToTabstripPref(profile_prefs);
#endif

  // Added 07/2026.
  profile_prefs->ClearPref(kObsoleteManagementProfileLastLogTime);

#if !BUILDFLAG(IS_ANDROID)
  // Added 07/2026.
  profile_prefs->ClearPref(prefs::kProjectsPanelEntrypointEnabled);
  profile_prefs->ClearPref(prefs::kProjectsPanelPinnedToTabstrip);
#endif

  // Added 08/2026.
  profile_prefs->ClearPref(kPrivacySandboxNotices);
  profile_prefs->ClearPref(kPrivacySandboxM1ConsentDecisionMade);
  profile_prefs->ClearPref(kPrivacySandboxM1EEANoticeAcknowledged);
  profile_prefs->ClearPref(kPrivacySandboxM1RowNoticeAcknowledged);
  profile_prefs->ClearPref(kPrivacySandboxM1RestrictedNoticeAcknowledged);
  profile_prefs->ClearPref(kPrivacySandboxM1PromptSuppressed);
  profile_prefs->ClearPref(kPrivacySandboxNoticeDisplayed);
  profile_prefs->ClearPref(kPrivacySandboxConsentDecisionMade);
  profile_prefs->ClearPref(kPrivacySandboxNoConfirmationSandboxDisabled);
  profile_prefs->ClearPref(kPrivacySandboxNoConfirmationSandboxRestricted);
  profile_prefs->ClearPref(kPrivacySandboxNoConfirmationSandboxManaged);
  profile_prefs->ClearPref(
      kPrivacySandboxNoConfirmationThirdPartyCookiesBlocked);
  profile_prefs->ClearPref(kPrivacySandboxNoConfirmationManuallyControlled);
  profile_prefs->ClearPref(kPrivacySandboxDisabledInsufficientConfirmation);
  profile_prefs->ClearPref(kPrivacySandboxTopicsConsentGiven);
  profile_prefs->ClearPref(kPrivacySandboxTopicsConsentLastUpdateTime);
  profile_prefs->ClearPref(kPrivacySandboxTopicsConsentLastUpdateReason);
  profile_prefs->ClearPref(kPrivacySandboxTopicsConsentTextAtLastUpdate);
  profile_prefs->ClearPref(kPrivacySandboxAllowNoticeFor3PCBlockedTrial);

  // Added 08/2026.
  profile_prefs->ClearPref(kObsoleteAutofillWalletImportEnabled);
  profile_prefs->ClearPref(kObsoleteAutofillWalletImportEnabledMigrated);
  profile_prefs->ClearPref(kPrivacySandboxTopicsDataAccessibleSince);
  profile_prefs->ClearPref(kPrivacySandboxBlockedTopics);
  profile_prefs->ClearPref(kPrivacySandboxFledgeJoinBlocked);

  // Added 08/2026.
  profile_prefs->ClearPref(kShowRollbackUiModeB);
  profile_prefs->ClearPref(kBlockAll3pcToggleEnabled);
  profile_prefs->ClearPref(kTrackingProtection3pcdEnabled);

  // Please don't delete the following line. It is used by PRESUBMIT.py.
  // END_MIGRATE_OBSOLETE_PROFILE_PREFS

  // IMPORTANT NOTE: This code is *not* run on iOS Chrome. If a pref is migrated
  // or cleared here, and that pref is also used in iOS Chrome, it may also need
  // to be migrated or cleared specifically for iOS as well. This could be by
  // doing the migration in feature code that's called by all platforms instead
  // of here, or by calling migration code in the appropriate place for iOS
  // specifically, e.g. ios/chrome/browser/shared/model/prefs/browser_prefs.mm.
}
