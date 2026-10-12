// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "chrome/browser/ui/global_media_controls/media_item_ui_device_selector_delegate.h"
#include "chrome/browser/ui/global_media_controls/media_notification_device_provider.h"
#include "components/global_media_controls/public/media_session_item_producer.h"
#include "components/global_media_controls/public/media_session_item_producer_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace global_media_controls {
class MediaItemManager;
}  // namespace global_media_controls

class MediaNotificationService
    : public KeyedService,
      public MediaItemUIDeviceSelectorDelegate,
      public global_media_controls::MediaSessionItemProducerObserver {
 public:
  MediaNotificationService(Profile* profile, bool show_from_all_profiles);
  MediaNotificationService(const MediaNotificationService&) = delete;
  MediaNotificationService& operator=(const MediaNotificationService&) = delete;
  ~MediaNotificationService() override;

  global_media_controls::MediaItemManager* media_item_manager() {
    return item_manager_.get();
  }

  // MediaItemUIDeviceSelectorDelegate:
  void OnAudioSinkChosen(const std::string& item_id,
                         const std::string& sink_id) override;
  base::CallbackListSubscription RegisterAudioOutputDeviceDescriptionsCallback(
      MediaNotificationDeviceProvider::GetOutputDevicesCallback callback)
      override;
  base::CallbackListSubscription
  RegisterIsAudioOutputDeviceSwitchingSupportedCallback(
      const std::string& id,
      base::RepeatingCallback<void(bool)> callback) override;

  // global_media_controls::MediaSessionItemProducerObserver:
  void OnMediaSessionActionButtonPressed(
      const std::string& id,
      media_session::mojom::MediaSessionAction action) override;

  void set_device_provider_for_testing(
      std::unique_ptr<MediaNotificationDeviceProvider> device_provider);

 private:
  friend class MediaNotificationProviderImplTest;
  friend class MediaNotificationServiceTest;
  friend class MediaToolbarButtonControllerTest;

  // Checks if the given `request_id` should always be blocked (i.e.
  // notification hidden). Mainly used for glic.
  bool IsIdBlocked(const std::string& request_id) const;

  std::unique_ptr<global_media_controls::MediaItemManager> item_manager_;

  std::unique_ptr<global_media_controls::MediaSessionItemProducer>
      media_session_item_producer_;

  // Generates a list of available audio devices.
  std::unique_ptr<MediaNotificationDeviceProvider> device_provider_;

  // Tracks the number of times we have recorded an action for a specific
  // source. We use this to cap the number of UKM recordings per site.
  std::map<ukm::SourceId, int> actions_recorded_to_ukm_;
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_MEDIA_NOTIFICATION_SERVICE_H_
