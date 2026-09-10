// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/web_api/web_printing_service_binder.h"

#include <utility>

#include "base/feature_list.h"
#include "content/public/browser/isolated_context_util.h"
#include "content/public/browser/render_frame_host.h"
#include "printing/buildflags/buildflags.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

namespace printing {

void CreateWebPrintingServiceForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebPrintingService> receiver) {
  if (!base::FeatureList::IsEnabled(blink::features::kWebPrinting)) {
    mojo::ReportBadMessage("The WebPrinting API is disabled.");
    return;
  }
  if (!render_frame_host->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kWebPrinting)) {
    mojo::ReportBadMessage(
        "Access to the feature \"web-printing\" is disallowed by permissions "
        "policy.");
    return;
  }
  // There are some security concerns around this API's fingerprinting surface
  // and the lack of motivating use cases outside of IWAs -- most users either
  // have no printers at all or only one which leads us to a 'take it or leave
  // it' situation where regular websites do not really need to access printer
  // information.
  // This decision might be reconsidered in the future, but for now we'll stick
  // to the IWA-only approach.
  if (!content::HasIsolatedContextCapability(render_frame_host)) {
    mojo::ReportBadMessage(
        "Frame is not sufficiently isolated to use the WebPrinting API.");
    return;
  }

  mojo::ReportBadMessage(
      "WebPrinting API is currently supported only on ChromeOS with CUPS "
      "enabled.");
}

}  // namespace printing
