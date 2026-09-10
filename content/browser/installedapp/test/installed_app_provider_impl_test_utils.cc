// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/installedapp/test/installed_app_provider_impl_test_utils.h"

#include "third_party/blink/public/mojom/installedapp/related_application.mojom.h"

namespace content {

blink::mojom::RelatedApplicationPtr CreateRelatedApplicationFromPlatformAndId(
    const std::string& platform,
    const std::string& id) {
  auto application = blink::mojom::RelatedApplication::New();
  application->platform = platform;
  application->id = id;
  return application;
}

FakeContentBrowserClientForQueryInstalledWebApps::
    FakeContentBrowserClientForQueryInstalledWebApps(
        std::vector<std::string> installed_web_app_ids) {
  for (const std::string& id : installed_web_app_ids) {
    GURL id_url(id);
    if (id_url.is_valid()) {
      installed_web_app_ids_.push_back(id_url);
    }
  }
}
FakeContentBrowserClientForQueryInstalledWebApps::
    ~FakeContentBrowserClientForQueryInstalledWebApps() = default;

#if !BUILDFLAG(IS_ANDROID)
void FakeContentBrowserClientForQueryInstalledWebApps::
    QueryInstalledWebAppsByManifestId(
        const GURL&,
        const GURL& id,
        content::BrowserContext*,
        base::OnceCallback<
            void(std::optional<blink::mojom::RelatedApplication>)> callback) {
  std::optional<blink::mojom::RelatedApplication> result;

  if (std::ranges::find(installed_web_app_ids_, id) !=
      installed_web_app_ids_.end()) {
    blink::mojom::RelatedApplication application;
    application.platform = "webapp";
    application.id = id.spec();
    result = application;
  }

  std::move(callback).Run(result);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace content
