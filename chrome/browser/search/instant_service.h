// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
#define CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/render_process_host_observer.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

// Tracks render process host IDs that are associated with Instant, i.e.
// processes that are used to render chrome-search: pages. Also adds the theme
// and favicon data sources of its profile.
class InstantService : public KeyedService,
                       public content::RenderProcessHostObserver {
 public:
  explicit InstantService(Profile* profile);

  InstantService(const InstantService&) = delete;
  InstantService& operator=(const InstantService&) = delete;

  ~InstantService() override;

  // Add RenderProcessHosts that are associated with Instant processes and query
  // based on PID.
  void AddInstantProcess(content::RenderProcessHost* host);
  bool IsInstantProcess(int process_id) const;

  // Determine if this chrome-search: request is coming from an Instant render
  // process.
  static bool ShouldServiceRequest(const GURL& url,
                                   content::BrowserContext* browser_context,
                                   int render_process_id);

#if defined(UNIT_TEST)
  int GetInstantProcessCount() const {
    return process_ids_.size();
  }
#endif

 private:
  // KeyedService:
  void Shutdown() override;

  // content::RenderProcessHostObserver:
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  const raw_ptr<Profile> profile_;

  // The process ids associated with Instant processes.
  std::set<int> process_ids_;

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      host_observation_{this};
};

#endif  // CHROME_BROWSER_SEARCH_INSTANT_SERVICE_H_
