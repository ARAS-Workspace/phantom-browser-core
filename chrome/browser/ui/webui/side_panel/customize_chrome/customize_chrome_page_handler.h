// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller_observer.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/webui_url_constants.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace new_tab_footer {
class NewTabFooterController;
}

class Profile;

/**
 * Places where the chrome web store can be opened from in Customize Chrome.
 * This enum must match the numbering for NTPChromeWebStoreOpen in enums.xml.
 * These values are persisted to logs. Entries should not be renumbered, removed
 * or reused.
 */
enum class NtpChromeWebStoreOpen {
  kAppearance = 0,
  kCollections = 1,
  kWritingEssentialsCollectionPage = 2,
  kWorkflowPlanningCategoryPage = 3,
  kShoppingCategoryPage = 4,
  kHomePage = 5,
  kMaxValue = kHomePage,
};

class CustomizeChromePageHandler
    : public side_panel::mojom::CustomizeChromePageHandler,
      public ui::NativeThemeObserver,
      public ThemeServiceObserver,
      public NtpCustomBackgroundServiceObserver,
      public new_tab_footer::NewTabFooterControllerObserver {
 public:
  // Returns whether the page handler can be constructed. Used to decide whether
  // the sidepanel should be allowed to show.
  static bool IsSupported(
      NtpCustomBackgroundService* ntp_custom_background_service,
      Profile* profile);

  CustomizeChromePageHandler(
      mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
          pending_page_handler,
      mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
      NtpCustomBackgroundService* ntp_custom_background_service,
      content::WebContents* web_contents,
      std::optional<base::RepeatingCallback<void(const GURL&)>>
          open_url_callback = std::nullopt);

  CustomizeChromePageHandler(const CustomizeChromePageHandler&) = delete;
  CustomizeChromePageHandler& operator=(const CustomizeChromePageHandler&) =
      delete;

  ~CustomizeChromePageHandler() override;

  // Passes ScrollToSection calls to the CustomizeChromePage.
  void ScrollToSection(CustomizeChromeSection section);

  // Passes AttachedTabStateUpdated calls to the CustomizeChromePage.
  void AttachedTabStateUpdated(const GURL& url);

  // side_panel::mojom::CustomizeChromePageHandler:
  void SetDefaultColor() override;
  void RemoveBackgroundImage() override;
  void UpdateTheme() override;
  void OpenNtpManagedByPage() override;
  void SetFooterVisible(bool visible) override;
  void UpdateFooterSettings() override;
  void UpdateScrollToSection() override;
  void UpdateAttachedTabState() override;
  void UpdateNtpManagedByName() override;

 private:
  // Returns the type of New Tab Page the SidePanel is attached to.
  side_panel::mojom::NewTabPageType GetNewTabPageType(const GURL& url);

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // ThemeServiceObserver:
  void OnThemeChanged() override;

  // NtpCustomBackgroundServiceObserver:
  void OnCustomBackgroundImageUpdated() override;

  // new_tab_footer::NewTabFooterControllerObserver:
  void OnFooterVisibilityUpdated(bool visible) override;

  // Called when the embedding BrowserWindowInterface has changed.
  void OnBrowserWindowInterfaceChanged();

  raw_ptr<NtpCustomBackgroundService> ntp_custom_background_service_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<ThemeService> theme_service_;

  // Caches a request to scroll to a section in case the front-end queries the
  // last requested section, e.g. during load.
  CustomizeChromeSection last_requested_section_ =
      CustomizeChromeSection::kUnspecified;

  // Caches the attached tab state provided to the handler, in cases where the
  // value needs to be requeried by the page.
  GURL last_source_url_{chrome::ChromeUINewTabPageURLAsGURL()};

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      native_theme_observation_{this};
  base::ScopedObservation<ThemeService, ThemeServiceObserver>
      theme_service_observation_{this};
  base::ScopedObservation<NtpCustomBackgroundService,
                          NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observation_{this};
  base::ScopedObservation<new_tab_footer::NewTabFooterController,
                          new_tab_footer::NewTabFooterControllerObserver>
      footer_controller_observation_{this};

  // Notifies this when the browser window context changes.
  base::CallbackListSubscription browser_window_changed_subscription_;

  mojo::Remote<side_panel::mojom::CustomizeChromePage> page_;
  mojo::Receiver<side_panel::mojom::CustomizeChromePageHandler> receiver_;

  // Callback used to open a URL.
  base::RepeatingCallback<void(const GURL&)> open_url_callback_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_CUSTOMIZE_CHROME_CUSTOMIZE_CHROME_PAGE_HANDLER_H_
