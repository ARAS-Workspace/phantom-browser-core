// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/select_file_policy/chrome_select_file_policy.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

void OpenWebPage(Profile* profile, const GURL& url) {
  NavigateParams navigate_params(profile, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

}  // namespace

// static
bool CustomizeChromePageHandler::IsSupported(
    NtpCustomBackgroundService* ntp_custom_background_service,
    Profile* profile) {
  if (!ntp_custom_background_service) {
    return false;
  }

  if (!ThemeServiceFactory::GetForProfile(profile)) {
    return false;
  }

  if (!NtpBackgroundServiceFactory::GetForProfile(profile)) {
    return false;
  }

  return true;
}

CustomizeChromePageHandler::CustomizeChromePageHandler(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler,
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents,
    std::optional<base::RepeatingCallback<void(const GURL&)>> open_url_callback)
    : ntp_custom_background_service_(ntp_custom_background_service),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile_)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_contents_,
              base::BindRepeating(
                  &CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      page_(std::move(pending_page)),
      receiver_(this, std::move(pending_page_handler)),
      open_url_callback_(open_url_callback.has_value()
                             ? open_url_callback.value()
                             : base::BindRepeating(&OpenWebPage, profile_)) {
  CHECK(IsSupported(ntp_custom_background_service_, profile_));

  ntp_background_service_->AddObserver(this);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_);

  OnBrowserWindowInterfaceChanged();

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());
}

CustomizeChromePageHandler::~CustomizeChromePageHandler() {
  if (ntp_background_service_) {
    ntp_background_service_->RemoveObserver(this);
  }
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void CustomizeChromePageHandler::ScrollToSection(
    CustomizeChromeSection section) {
  last_requested_section_ = section;
  if (section == CustomizeChromeSection::kUnspecified) {
    // Cannot scroll to unspecified section.
    return;
  }
  page_->ScrollToSection(section);
}

void CustomizeChromePageHandler::AttachedTabStateUpdated(const GURL& url) {
  last_source_url_ = url;
  page_->AttachedTabStateUpdated(GetNewTabPageType(url));
}

void CustomizeChromePageHandler::SetDefaultColor() {
  theme_service_->UseDeviceTheme(false);
  theme_service_->UseDefaultTheme();
}

void CustomizeChromePageHandler::SetFollowDeviceTheme(bool follow) {
  theme_service_->UseDeviceTheme(follow);
}

void CustomizeChromePageHandler::SetBackgroundImage(
    const std::string& attribution_1,
    const std::string& attribution_2,
    const GURL& attribution_url,
    const GURL& image_url,
    const GURL& thumbnail_url,
    const std::string& collection_id) {
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      image_url, thumbnail_url, attribution_1, attribution_2, attribution_url,
      collection_id);
  customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
}

void CustomizeChromePageHandler::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  // Only populating the |collection_id| turns on refresh daily which overrides
  // the the selected image.
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* thumbnail_url */ GURL(),
      /* attribution_line_1= */ "", /* attribution_line_2= */ "",
      /* action_url= */ GURL(), collection_id);
  customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
}

void CustomizeChromePageHandler::GetBackgroundCollections(
    GetBackgroundCollectionsCallback callback) {
  if (!ntp_background_service_ || background_collections_callback_) {
    std::move(callback).Run(
        std::vector<side_panel::mojom::BackgroundCollectionPtr>());
    return;
  }
  background_collections_request_start_time_ = base::TimeTicks::Now();
  background_collections_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionInfo();
}

void CustomizeChromePageHandler::GetReplacementCollectionPreviewImage(
    const std::string& collection_id,
    GetReplacementCollectionPreviewImageCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         std::nullopt);
  if (!ntp_background_service_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  ntp_background_service_->FetchReplacementCollectionPreviewImage(
      collection_id, std::move(callback));
}

void CustomizeChromePageHandler::GetBackgroundImages(
    const std::string& collection_id,
    GetBackgroundImagesCallback callback) {
  if (background_images_callback_) {
    std::move(background_images_callback_)
        .Run(std::vector<side_panel::mojom::CollectionImagePtr>());
  }
  if (!ntp_background_service_) {
    std::move(callback).Run(
        std::vector<side_panel::mojom::CollectionImagePtr>());
    return;
  }
  images_request_collection_id_ = collection_id;
  background_images_request_start_time_ = base::TimeTicks::Now();
  background_images_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionImageInfo(collection_id);
}

void CustomizeChromePageHandler::ChooseLocalCustomBackground(
    ChooseLocalCustomBackgroundCallback callback) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    std::move(callback).Run(false);
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("gif"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));
  DCHECK(!choose_local_custom_background_callback_);
  choose_local_custom_background_callback_ = std::move(callback);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      profile_->last_selected_directory(), &file_types, 0,
      base::FilePath::StringType(), web_contents_->GetTopLevelNativeWindow(),
      nullptr);
}

void CustomizeChromePageHandler::RemoveBackgroundImage() {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->ResetCustomBackgroundInfo();
    customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
  }
}

void CustomizeChromePageHandler::UpdateTheme() {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->RefreshBackgroundIfNeeded();
  }
  auto theme = side_panel::mojom::Theme::New();
  auto custom_background =
      ntp_custom_background_service_
          ? ntp_custom_background_service_->GetCustomBackground()
          : std::nullopt;
  auto background_image = side_panel::mojom::BackgroundImage::New();
  if (custom_background.has_value()) {
    background_image->url = custom_background->custom_background_url;
    background_image->snapshot_url =
        custom_background->custom_background_snapshot_url;
    background_image->is_uploaded_image = custom_background->is_uploaded_image;
    background_image->local_background_id =
        custom_background->local_background_id;
    background_image->title =
        custom_background->custom_background_attribution_line_1;
    background_image->collection_id = custom_background->collection_id;
    background_image->daily_refresh_enabled =
        custom_background->daily_refresh_enabled;
  } else {
    background_image = nullptr;
  }
  theme->background_image = std::move(background_image);
  theme->follow_device_theme = theme_service_->UsingDeviceTheme();

  auto user_color = theme_service_->GetUserColor();
  // If a user has a GM2 theme set they are in a limbo state between the 2 theme
  // types. We need to get the color of their theme with
  // GetAutogeneratedThemeColor still until they set a GM3 theme, use the old
  // way of detecting default, and use the old color tokens to keep an accurate
  // representation of what the user is seeing.
  if (user_color.has_value()) {
    theme->background_color =
        web_contents_->GetColorProvider().GetColor(ui::kColorSysInversePrimary);
    if (user_color.value() != SK_ColorTRANSPARENT) {
      theme->foreground_color = theme->background_color;
    }
  } else {
    theme->background_color =
        web_contents_->GetColorProvider().GetColor(kColorNewTabPageBackground);
    if (!theme_service_->UsingDefaultTheme() &&
        !theme_service_->UsingSystemTheme()) {
      theme->foreground_color =
          web_contents_->GetColorProvider().GetColor(ui::kColorFrameActive);
    }
  }
  theme->background_managed_by_policy =
      ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy();
  if (theme_service_->UsingExtensionTheme()) {
    const extensions::Extension* theme_extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetByID(theme_service_->GetThemeID());
    if (theme_extension) {
      auto third_party_theme_info =
          side_panel::mojom::ThirdPartyThemeInfo::New();
      third_party_theme_info->id = theme_extension->id();
      third_party_theme_info->name = theme_extension->name();
      theme->third_party_theme_info = std::move(third_party_theme_info);
    }
  }
  page_->SetTheme(std::move(theme));
}

void CustomizeChromePageHandler::UpdateThemeEditable(bool is_theme_editable) {
  page_->SetThemeEditable(is_theme_editable);
}

void CustomizeChromePageHandler::OpenNtpManagedByPage() {
  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    open_url_callback_.Run(
        net::AppendOrReplaceQueryParameter(GURL(chrome::kChromeUIExtensionsURL),
                                           "id", extension_managing_ntp->id()));
  }
}

void CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  footer_controller_observation_.Reset();
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  footer_controller_observation_.Observe(footer_controller);
}

void CustomizeChromePageHandler::SetFooterVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, visible);
}

void CustomizeChromePageHandler::UpdateFooterSettings() {
  page_->SetFooterSettings(
      profile_->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible),
      profile_->GetPrefs()->GetBoolean(
          prefs::kNTPFooterExtensionAttributionEnabled));
}

void CustomizeChromePageHandler::UpdateScrollToSection() {
  ScrollToSection(last_requested_section_);
}

void CustomizeChromePageHandler::UpdateAttachedTabState() {
  AttachedTabStateUpdated(last_source_url_);
}

void CustomizeChromePageHandler::UpdateNtpManagedByName() {
  std::string name;
  std::string description;

  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    name = extension_managing_ntp->short_name();
    description = l10n_util::GetStringUTF8(IDS_NTP_MANAGED_BY_EXTENSION);
  }

  page_->NtpManagedByNameUpdated(name, description);
}

void CustomizeChromePageHandler::LogEvent(NTPLoggingEventType event) {
  switch (event) {
    case NTP_BACKGROUND_UPLOAD_CANCEL:
      base::RecordAction(base::UserMetricsAction(
          "NTPRicherPicker.Backgrounds.UploadCanceled"));
      break;
    case NTP_BACKGROUND_UPLOAD_DONE:
      base::RecordAction(base::UserMetricsAction(
          "NTPRicherPicker.Backgrounds.UploadConfirmed"));
      break;
    default:
      break;
  }
}

void CustomizeChromePageHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnThemeChanged() {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void CustomizeChromePageHandler::OnCollectionInfoAvailable() {
  if (!background_collections_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_collections_request_start_time_;
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Collections.RequestLatency", duration);
  // Any response where no collections are returned is considered a failure.
  if (ntp_background_service_->collection_info().empty()) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
        duration);
  } else {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Success",
        duration);
  }

  std::vector<side_panel::mojom::BackgroundCollectionPtr> collections;
  for (const auto& info : ntp_background_service_->collection_info()) {
    auto collection = side_panel::mojom::BackgroundCollection::New();
    collection->id = info.collection_id;
    collection->label = info.collection_name;
    collection->preview_image_url = GURL(info.preview_image_url);
    collections.push_back(std::move(collection));
  }
  std::move(background_collections_callback_).Run(std::move(collections));
}

void CustomizeChromePageHandler::OnCollectionImagesAvailable() {
  if (!background_images_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_images_request_start_time_;
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Images.RequestLatency", duration);
  // Any response where no images are returned is considered a failure.
  if (ntp_background_service_->collection_images().empty()) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Failure", duration);
  } else {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Success", duration);
  }

  std::vector<side_panel::mojom::CollectionImagePtr> images;
  if (ntp_background_service_->collection_images().empty()) {
    std::move(background_images_callback_).Run(std::move(images));
    return;
  }
  auto collection_id =
      ntp_background_service_->collection_images()[0].collection_id;
  for (const auto& info : ntp_background_service_->collection_images()) {
    DCHECK(info.collection_id == collection_id);
    auto image = side_panel::mojom::CollectionImage::New();
    image->attribution_1 = !info.attribution.empty() ? info.attribution[0] : "";
    image->attribution_2 =
        info.attribution.size() > 1 ? info.attribution[1] : "";
    image->attribution_url = info.attribution_action_url;
    image->image_url = info.image_url;
    image->preview_image_url = info.thumbnail_image_url;
    image->collection_id = collection_id;
    images.push_back(std::move(image));
  }
  std::move(background_images_callback_).Run(std::move(images));
}

void CustomizeChromePageHandler::OnNextCollectionImageAvailable() {}

void CustomizeChromePageHandler::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}

void CustomizeChromePageHandler::OnFooterVisibilityUpdated(bool visible) {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }
  UpdateFooterSettings();
}

void CustomizeChromePageHandler::FileSelected(const ui::SelectedFileInfo& file,
                                              int index) {
  DCHECK(choose_local_custom_background_callback_);
  if (ntp_custom_background_service_) {
    // Use the default theme color if wallpaper search is disabled.
    // If wallpaper search is enabled, |ntp_custom_background_service_|
    // will handle setting the theme color.
    if (!base::FeatureList::IsEnabled(
            ntp_features::kCustomizeChromeWallpaperSearch) ||
        !base::FeatureList::IsEnabled(
            optimization_guide::features::kOptimizationGuideModelExecution)) {
      theme_service_->UseDefaultTheme();
    }

    profile_->set_last_selected_directory(file.path().DirName());
    ntp_custom_background_service_->SelectLocalBackgroundImage(file.path());
    customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
  }
  select_file_dialog_ = nullptr;
  LogEvent(NTP_BACKGROUND_UPLOAD_DONE);
  std::move(choose_local_custom_background_callback_).Run(true);
}

void CustomizeChromePageHandler::FileSelectionCanceled() {
  DCHECK(choose_local_custom_background_callback_);
  select_file_dialog_ = nullptr;
  LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL);
  std::move(choose_local_custom_background_callback_).Run(false);
}

side_panel::mojom::NewTabPageType CustomizeChromePageHandler::GetNewTabPageType(
    const GURL& url) {
  if (ntp_footer::IsExtensionNtp(url, profile_)) {
    return side_panel::mojom::NewTabPageType::kExtension;
  } else if (NewTabPageThirdPartyUI::IsNewTabPageOrigin(url)) {
    return side_panel::mojom::NewTabPageType::kThirdPartyWebUI;
  } else if (NewTabUI::IsNewTab(url)) {
    return profile_->IsGuestSession()
               ? side_panel::mojom::NewTabPageType::kGuestMode
               : side_panel::mojom::NewTabPageType::kIncognito;
  }

  return side_panel::mojom::NewTabPageType::kNone;
}
