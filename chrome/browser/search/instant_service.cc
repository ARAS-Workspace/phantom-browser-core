// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"

InstantService::InstantService(Profile* profile) : profile_(profile) {
  // The initialization below depends on a typical set of browser threads. Skip
  // it if we are running in a unit test without the full suite.
  if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
    return;

  // TODO(crbug.com/40757220): multiple WebUI pages depend on the theme source
  // without adding it themselves. This is not causing an issue because the
  // theme source is being added here. The source should be added where it is
  // used and then the following can be removed.
  content::URLDataSource::Add(profile_,
                              std::make_unique<ThemeSource>(profile_));

  // Set up the legacy favicon data source.
  content::URLDataSource::Add(
      profile_, std::make_unique<FaviconSource>(
                    profile_, chrome::FaviconUrlFormat::kFaviconLegacy));
}

InstantService::~InstantService() = default;

void InstantService::AddInstantProcess(content::RenderProcessHost* host) {
  process_ids_.insert(host->GetDeprecatedID());
  // The same process may be added for multiple WebContents. Only observe once.
  if (!host_observation_.IsObservingSource(host)) {
    host_observation_.AddObservation(host);
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::Shutdown() {
  process_ids_.clear();
}

void InstantService::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  Profile* renderer_profile = static_cast<Profile*>(host->GetBrowserContext());
  if (profile_ == renderer_profile) {
    process_ids_.erase(host->GetDeprecatedID());
    host_observation_.RemoveObservation(host);
  }
}

// static
bool InstantService::ShouldServiceRequest(
    const GURL& url,
    content::BrowserContext* browser_context,
    int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* instant_service = InstantServiceFactory::GetForProfile(
      static_cast<Profile*>(browser_context));

  if (!instant_service)
    return false;

  // The process_id for the navigation request will be -1. If
  // so, allow this request since it's not going to another renderer.
  return render_process_id == -1 ||
         instant_service->IsInstantProcess(render_process_id);
}
