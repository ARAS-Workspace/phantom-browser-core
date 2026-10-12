// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/global_media_controls/media_notification_service.h"

#include <algorithm>
#include <memory>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider_impl.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/buildflags.h"
#include "components/global_media_controls/public/media_item_manager.h"
#include "components/global_media_controls/public/media_item_producer.h"
#include "components/media_message_center/media_notification_item.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/media_session_service.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace {

// The maximum number of actions we will record to UKM for a specific source.
constexpr int kMaxActionsRecordedToUKM = 100;

// Here we check to see if the WebContents is focused. Note that we can't just
// use |WebContentsObserver::OnWebContentsFocused()| and
// |WebContentsObserver::OnWebContentsLostFocus()| because focusing the
// MediaDialogView causes the WebContents to "lose focus", so we'd never be
// focused.
bool IsWebContentsFocused(content::WebContents* web_contents) {
  DCHECK(web_contents);
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->FindBrowserWithTab(web_contents);
  if (!browser) {
    return false;
  }
  // If the given WebContents is not in the focused window, then it's not
  // focused. Note that we know a Browser is focused because otherwise the user
  // could not interact with the MediaDialogView.
  if (GetLastActiveBrowserWindowInterfaceWithAnyProfile() != browser) {
    return false;
  }
  return browser->GetTabStripModel()->GetActiveWebContents() == web_contents;
}

}  // namespace

MediaNotificationService::MediaNotificationService(Profile* profile,
                                                   bool show_from_all_profiles) {
  item_manager_ = global_media_controls::MediaItemManager::Create();

  std::optional<base::UnguessableToken> source_id;
  if (!show_from_all_profiles) {
    source_id = content::MediaSession::GetSourceId(profile);
  }

  mojo::Remote<media_session::mojom::AudioFocusManager> audio_focus_remote;
  mojo::Remote<media_session::mojom::MediaControllerManager>
      controller_manager_remote;

  // Connect to receive audio focus events.
  content::GetMediaSessionService().BindAudioFocusManager(
      audio_focus_remote.BindNewPipeAndPassReceiver());

  // Connect to the controller manager so we can create media controllers for
  // media sessions.
  content::GetMediaSessionService().BindMediaControllerManager(
      controller_manager_remote.BindNewPipeAndPassReceiver());

  media_session_item_producer_ =
      std::make_unique<global_media_controls::MediaSessionItemProducer>(
          std::move(audio_focus_remote), std::move(controller_manager_remote),
          item_manager_.get(), source_id);

  // It is safe to use `base::Unretained` here because
  // `media_session_item_producer_` is owned by `this`.
  media_session_item_producer_->SetIsIdBlockedCallback(base::BindRepeating(
      &MediaNotificationService::IsIdBlocked, base::Unretained(this)));

  media_session_item_producer_->AddObserver(this);
  item_manager_->AddItemProducer(media_session_item_producer_.get());
}

MediaNotificationService::~MediaNotificationService() {
  media_session_item_producer_->RemoveObserver(this);
  item_manager_->RemoveItemProducer(media_session_item_producer_.get());
}

void MediaNotificationService::OnAudioSinkChosen(const std::string& item_id,
                                                 const std::string& sink_id) {
  media_session_item_producer_->SetAudioSinkId(item_id, sink_id);
}

base::CallbackListSubscription
MediaNotificationService::RegisterAudioOutputDeviceDescriptionsCallback(
    MediaNotificationDeviceProvider::GetOutputDevicesCallback callback) {
  if (!device_provider_) {
    device_provider_ = std::make_unique<MediaNotificationDeviceProviderImpl>(
        content::CreateAudioSystemForAudioService());
  }
  return device_provider_->RegisterOutputDeviceDescriptionsCallback(
      std::move(callback));
}

base::CallbackListSubscription
MediaNotificationService::RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
    const std::string& id,
    base::RepeatingCallback<void(bool)> callback) {
  return media_session_item_producer_
      ->RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
          id, std::move(callback));
}

void MediaNotificationService::OnMediaSessionActionButtonPressed(
    const std::string& id,
    media_session::mojom::MediaSessionAction action) {
  auto* web_contents = content::MediaSession::GetWebContentsFromRequestId(id);
  if (!web_contents) {
    return;
  }
  base::UmaHistogramBoolean("Media.GlobalMediaControls.UserActionFocus",
                            IsWebContentsFocused(web_contents));

  ukm::UkmRecorder* recorder = ukm::UkmRecorder::Get();
  ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  if (++actions_recorded_to_ukm_[source_id] > kMaxActionsRecordedToUKM) {
    return;
  }
  ukm::builders::Media_GlobalMediaControls_ActionButtonPressed(source_id)
      .SetMediaSessionAction(static_cast<int64_t>(action))
      .Record(recorder);
}

void MediaNotificationService::set_device_provider_for_testing(
    std::unique_ptr<MediaNotificationDeviceProvider> device_provider) {
  device_provider_ = std::move(device_provider);
}

bool MediaNotificationService::IsIdBlocked(
    const std::string& request_id) const {
  return false;
}
