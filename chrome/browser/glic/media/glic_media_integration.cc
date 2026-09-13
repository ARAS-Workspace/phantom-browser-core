// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/media/glic_media_integration.h"

#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/media/glic_media_context.h"
#include "chrome/browser/glic/media/glic_media_page_cache.h"
#include "chrome/browser/glic/media/media_transcript_provider_impl.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/pref_names.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/peer_connection_tracker_host_observer.h"
#include "content/public/browser/web_contents.h"

namespace {


class GlicMediaPeerConnectionObserver
    : public content::PeerConnectionTrackerHostObserver {
 public:
  ~GlicMediaPeerConnectionObserver() override = default;

  void OnPeerConnectionAdded(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid,
      base::ProcessId pid,
      const std::string& url,
      const std::string& rtc_configuration) override {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (!rfh) {
      return;
    }

    auto* wc = content::WebContents::FromRenderFrameHost(rfh);
    if (!wc) {
      return;
    }

    // Attribute this to all frames in the WebContents.
    wc->ForEachRenderFrameHost([](content::RenderFrameHost* rfh) {
      if (auto* context =
              glic::GlicMediaContext::GetOrCreateForCurrentDocument(rfh)) {
        context->OnPeerConnectionAdded();
      }
    });

    // Also attribute this to its opener WebContents if this is a document PiP
    // window.
    if (auto* pip_window_manager =
            PictureInPictureWindowManager::GetInstance()) {
      auto* opener_wc = pip_window_manager->GetWebContents();
      if (opener_wc) {
        if (auto* context =
                glic::GlicMediaContext::GetOrCreateForCurrentDocument(
                    opener_wc->GetPrimaryMainFrame())) {
          context->OnPeerConnectionAdded();
        }
      }
    }
  }

  void OnPeerConnectionRemoved(
      content::GlobalRenderFrameHostId render_frame_host_id,
      int lid) override {
    auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id);
    if (!rfh) {
      return;
    }

    auto* wc = content::WebContents::FromRenderFrameHost(rfh);
    if (!wc) {
      return;
    }

    // Attribute this to all frames in the WebContents.
    wc->ForEachRenderFrameHost([](content::RenderFrameHost* rfh) {
      if (auto* context = glic::GlicMediaContext::GetForCurrentDocument(rfh)) {
        context->OnPeerConnectionRemoved();
      }
    });

    // Also attribute this to its opener WebContents if this is a document PiP
    // window.
    if (auto* pip_window_manager =
            PictureInPictureWindowManager::GetInstance()) {
      auto* opener_wc = pip_window_manager->GetWebContents();
      if (opener_wc) {
        if (auto* context = glic::GlicMediaContext::GetForCurrentDocument(
                opener_wc->GetPrimaryMainFrame())) {
          context->OnPeerConnectionRemoved();
        }
      }
    }
  }
};

class GlicMediaIntegrationImpl : public glic::GlicMediaIntegration,
                                 public base::SupportsUserData::Data {
 public:
  explicit GlicMediaIntegrationImpl(Profile*);
  ~GlicMediaIntegrationImpl() override = default;

  // glic::GlicMediaIntegration:
  void AppendContext(
      content::WebContents* web_contents,
      optimization_guide::proto::ContentNode* context_root) override;
  void AppendContextForFrame(
      content::RenderFrameHost* rfh,
      optimization_guide::proto::ContentNode* context_root) override;
  void OnPeerConnectionAddedForTesting(content::RenderFrameHost*) override;
  void OnPeerConnectionRemovedForTesting(content::RenderFrameHost*) override;
  void SetExcludedOrigins(
      const std::vector<url::Origin>& excluded_origins) override;
  bool IsInitializedForTesting() const override;  // IN-TEST

  // GlicMediaIntegrationImpl:
  void OnContextUpdated(glic::GlicMediaContext* context);

  // Returns whether `web_contents` should be excluded by origin checks.  This
  // includes subframes.
  bool IsExcludedByOrigin(content::WebContents* web_contents);

 protected:
  void Initialize();
  void OnConsentChanged();

  raw_ptr<Profile> profile_;
  // Don't let the transcript grow unbounded.
  static constexpr size_t max_size_bytes_ = 20000;
  glic::GlicMediaPageCache page_cache_;

  std::unique_ptr<GlicMediaPeerConnectionObserver> rtc_observer_;
  std::vector<url::Origin> excluded_origins_;
  base::CallbackListSubscription subscription_;

 private:
  void OnPrefChanged();

  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<GlicMediaIntegrationImpl> weak_ptr_factory_{this};
};


GlicMediaIntegrationImpl::GlicMediaIntegrationImpl(Profile* profile)
    : profile_(profile) {
  if (glic::GlicEnabling::HasConsentedForProfile(profile_)) {
    Initialize();
  } else {
    auto* keyed_service = glic::GlicKeyedService::Get(profile);
    if (keyed_service) {
      subscription_ = keyed_service->enabling().RegisterOnConsentChanged(
          base::BindRepeating(&GlicMediaIntegrationImpl::OnConsentChanged,
                              base::Unretained(this)));
    }
  }
}

void GlicMediaIntegrationImpl::Initialize() {
  // Initialization should only happen once.
  CHECK(!rtc_observer_);

  rtc_observer_ = std::make_unique<GlicMediaPeerConnectionObserver>();

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      glic::prefs::kGlicMediaUnderstandingEnabled,
      base::BindRepeating(&GlicMediaIntegrationImpl::OnPrefChanged,
                          base::Unretained(this)));

  OnPrefChanged();

  // For now, enable the pref if we get this far.  Do this after getting the
  // Live Caption controller, since it resets the pref to false.
  profile_->GetPrefs()->SetBoolean(prefs::kHeadlessCaptionEnabled, true);

  // Default to turning off for YT.
  std::vector<url::Origin> excluded_origins = {
      url::Origin::Create(GURL("https://www.youtube.com")),
      url::Origin::Create(GURL("http://www.youtube.com"))};
  SetExcludedOrigins(std::move(excluded_origins));
}

void GlicMediaIntegrationImpl::OnPrefChanged() {
  bool enabled = profile_->GetPrefs()->GetBoolean(
      glic::prefs::kGlicMediaUnderstandingEnabled);
  if (!enabled) {
    // Discard all transcripts when disabled.
    for (base::LinkNode<glic::GlicMediaPageCache::Entry>* node =
             page_cache_.head();
         node != page_cache_.end(); node = node->next()) {
      auto* entry = static_cast<glic::GlicMediaPageCache::Entry*>(node);
      auto* context = static_cast<glic::GlicMediaContext*>(entry);
      context->ClearAllTranscripts();
    }
  }
}

bool GlicMediaIntegrationImpl::IsExcludedByOrigin(
    content::WebContents* web_contents) {
  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  // Walk the frame tree.
  bool exclude_this = false;
  const auto& excluded_origins = excluded_origins_;
  rfh->ForEachRenderFrameHostWithAction(
      [&exclude_this, &excluded_origins](content::RenderFrameHost* rfh) {
        auto& origin = rfh->GetLastCommittedOrigin();
        for (auto& excluded_origin : excluded_origins) {
          exclude_this |= origin == excluded_origin;
        }
        return exclude_this
                   ? content::RenderFrameHost::FrameIterationAction::kStop
                   : content::RenderFrameHost::FrameIterationAction::kContinue;
      });

  return exclude_this;
}

void GlicMediaIntegrationImpl::OnConsentChanged() {
  if (glic::GlicEnabling::HasConsentedForProfile(profile_)) {
    Initialize();
    // Unsubscribe.
    subscription_ = {};
  }
}

void GlicMediaIntegrationImpl::AppendContext(
    content::WebContents* web_contents,
    optimization_guide::proto::ContentNode* context_root) {
  if (!web_contents) {
    return;
  }
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kAnnotatedPageContentWithMediaData)) {
    return;
  }
  // Walk the tree and find a transcript.
  content::RenderFrameHost* rfh = nullptr;
  web_contents->ForEachRenderFrameHost([&rfh](content::RenderFrameHost* host) {
    auto* context = glic::GlicMediaContext::GetForCurrentDocument(host);
    if (context && context->HasTranscriptChunks()) {
      rfh = host;
    }
  });
  if (rfh) {
    AppendContextForFrame(rfh, context_root);
  }
}

void GlicMediaIntegrationImpl::AppendContextForFrame(
    content::RenderFrameHost* rfh,
    optimization_guide::proto::ContentNode* context_root) {
  if (!rfh) {
    return;
  }
  context_root->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);

  if (IsExcludedByOrigin(content::WebContents::FromRenderFrameHost(rfh))) {
    return;
  }

  auto* context = glic::GlicMediaContext::GetForCurrentDocument(rfh);
  if (!context) {
    return;
  }

  auto chunks = context->GetTranscriptChunks();
  if (chunks.empty()) {
    return;
  }
  std::vector<std::string_view> pieces;
  pieces.reserve(chunks.size());
  for (const auto& chunk : chunks) {
    pieces.push_back(chunk.text);
  }

  std::string result = base::JoinString(pieces, "");

  // Trim to `max_size_bytes_`.  Note that we should utf8-trim.
  if (size_t result_size = result.length()) {
    if (result_size > max_size_bytes_) {
      // Remove the beginning of the result, leaving the end.
      size_t start_index = result_size - max_size_bytes_;
      // Ensure we don't cut in the middle of a UTF-8 multi-byte character.
      // UTF-8 continuation bytes start with 10xxxxxx (0x80 to 0xBF).
      while (start_index < result_size &&
             (static_cast<unsigned char>(result[start_index]) & 0xC0) == 0x80) {
        start_index++;
      }
      result = std::move(result).substr(start_index);
    }
  }

  // Include the entire context in one node.  This could be split into multiple
  // nodes too.
  context_root->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content(std::move(result));
}

void GlicMediaIntegrationImpl::OnContextUpdated(
    glic::GlicMediaContext* context) {
  page_cache_.PlaceAtFront(context);
}


void GlicMediaIntegrationImpl::OnPeerConnectionAddedForTesting(
    content::RenderFrameHost* rfh) {
  auto id = rfh->GetGlobalId();
  rtc_observer_->OnPeerConnectionAdded(id, /*lid=*/0, /*pid=*/{}, /*url=*/"",
                                       /*rtc_configuration=*/"");
}

void GlicMediaIntegrationImpl::OnPeerConnectionRemovedForTesting(
    content::RenderFrameHost* rfh) {
  auto id = rfh->GetGlobalId();
  rtc_observer_->OnPeerConnectionRemoved(id, /*lid=*/0);
}

void GlicMediaIntegrationImpl::SetExcludedOrigins(
    const std::vector<url::Origin>& excluded_origins) {
  excluded_origins_ = excluded_origins;
}

bool GlicMediaIntegrationImpl::IsInitializedForTesting() const {
  return !!rtc_observer_;
}

}  // namespace

namespace glic {

// static
GlicMediaIntegration* GlicMediaIntegration::GetFor(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  auto* integration =
      GetFor(Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  if (!integration) {
    return nullptr;
  }

  if (!optimization_guide::MediaTranscriptProvider::GetFor(web_contents)) {
    optimization_guide::MediaTranscriptProvider::SetFor(
        web_contents, std::make_unique<glic::MediaTranscriptProviderImpl>());
  }

  return integration;
}

GlicMediaIntegration* GlicMediaIntegration::GetFor(Profile* profile) {
  // The transcript source was removed with the speech recognition surface.
  return nullptr;
}

}  // namespace glic
