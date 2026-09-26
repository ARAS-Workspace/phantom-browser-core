// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"

#include "build/build_config.h"
#include "chrome/browser/content_settings/chrome_content_settings_utils.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/blocked_content/popup_navigation_delegate.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

ChromePopupNavigationDelegate::ChromePopupNavigationDelegate(
    NavigateParams params)
    : params_(std::move(params)),
      original_user_gesture_(params_.user_gesture) {}

content::RenderFrameHost* ChromePopupNavigationDelegate::GetOpener() {
  return params_.opener;
}

bool ChromePopupNavigationDelegate::GetOriginalUserGesture() {
  return original_user_gesture_;
}

GURL ChromePopupNavigationDelegate::GetURL() {
  return params_.url;
}

blocked_content::PopupNavigationDelegate::NavigateResult
ChromePopupNavigationDelegate::NavigateWithGesture(
    const blink::mojom::WindowFeatures& window_features,
    std::optional<WindowOpenDisposition> updated_disposition) {
  params_.user_gesture = true;
  if (updated_disposition) {
    params_.disposition = updated_disposition.value();
  }
  ::Navigate(&params_);
  if (params_.navigated_or_inserted_contents &&
      params_.disposition == WindowOpenDisposition::NEW_POPUP) {
    content::RenderFrameHost* host =
        params_.navigated_or_inserted_contents->GetPrimaryMainFrame();
    DCHECK(host);
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> client;
    host->GetRemoteAssociatedInterfaces()->GetInterface(&client);
    client->SetWindowFeatures(window_features.Clone());
  }
  return NavigateResult{params_.navigated_or_inserted_contents,
                        params_.disposition};
}

void ChromePopupNavigationDelegate::OnPopupBlocked(
    content::WebContents* web_contents,
    int total_popups_blocked_on_page) {}
