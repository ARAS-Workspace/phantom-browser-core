// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <string>

#include "base/trace_event/typed_macros.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_client.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

// Helpers --------------------------------------------------------------------

class ChromeOmniboxNavigationObserver;

namespace {

// HTTP 2xx, 401, and 407 all indicate that the target address exists.
bool ResponseCodeIndicatesSuccess(int response_code) {
  return ((response_code / 100) == 2) || (response_code == 401) ||
         (response_code == 407);
}

// Helper to keep ChromeOmniboxNavigationObserver alive while the initiated
// navigation is pending.
struct NavigationUserData
    : public content::NavigationHandleUserData<NavigationUserData> {
  NavigationUserData(content::NavigationHandle& navigation,
                     scoped_refptr<ChromeOmniboxNavigationObserver> observer)
      : observer(std::move(observer)) {}
  ~NavigationUserData() override = default;

  scoped_refptr<ChromeOmniboxNavigationObserver> observer;

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationUserData);

}  // namespace

ChromeOmniboxNavigationObserver::ChromeOmniboxNavigationObserver(
    content::NavigationHandle& navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match)
    : content::WebContentsObserver(navigation.GetWebContents()),
      text_(text),
      match_(match),
      navigation_id_(navigation.GetNavigationId()),
      profile_(profile) {
  NavigationUserData::CreateForNavigationHandle(navigation, this);
}

ChromeOmniboxNavigationObserver::~ChromeOmniboxNavigationObserver() = default;

void ChromeOmniboxNavigationObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetNavigationId() != navigation_id_) {
    return;
  }

  // This is the navigation we've started ourselves in the primary main frame
  // of the WebContents.
  DCHECK(navigation_handle->IsInPrimaryMainFrame());

  // Ignore navigations which didn't commit, or committed a page which bypassed
  // the network (e.g. about:blank).
  if (!navigation_handle->HasCommitted() ||
      !navigation_handle->GetResponseHeaders()) {
    return;
  }

  if (ResponseCodeIndicatesSuccess(
          navigation_handle->GetResponseHeaders()->response_code())) {
    ChromeOmniboxClient::OnSuccessfulNavigation(profile_, text_, match_);
  }

  if (navigation_handle->GetResponseHeaders()->response_code() == 404) {
    On404();
  }
}

void ChromeOmniboxNavigationObserver::On404() {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  const TemplateURL* template_url = match_.GetTemplateURL(template_url_service);
  // If the omnibox navigation was to a URL (and hence did not involve a
  // TemplateURL / search at all) or the invoked search engine has been
  // deleted or otherwise modified, doing nothing is the right thing.
  if (template_url == nullptr) {
    return;
  }
  // If there's any hint that we should keep this search engine around, don't
  // mess with it.
  if (template_url_service->ShowInDefaultList(template_url) ||
      !template_url->safe_for_autoreplace() ||
      template_url->starter_pack_id() !=
          template_url_starter_pack_data::StarterPackId::kNone) {
    return;
  }
  // This custom search engine is safe to delete.
  template_url_service->Remove(template_url);
}

// static
void ChromeOmniboxNavigationObserver::Create(
    content::NavigationHandle* navigation,
    Profile* profile,
    const std::u16string& text,
    const AutocompleteMatch& match) {
  TRACE_EVENT("omnibox", "ChromeOmniboxNavigationObserver::Create",
              "navigation", navigation, "match", match);

  if (!navigation) {
    return;
  }

  // The observer will be kept alive until the navigation finishes.
  new ChromeOmniboxNavigationObserver(*navigation, profile, text, match);
}
