// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager_basic.h"

#include "build/build_config.h"

namespace printing {

PrintViewManagerBasic::PrintViewManagerBasic(content::WebContents* web_contents)
    : PrintViewManagerBase(web_contents),
      content::WebContentsUserData<PrintViewManagerBasic>(*web_contents) {}

PrintViewManagerBasic::~PrintViewManagerBasic() {}

// static
void PrintViewManagerBasic::BindPrintManagerHost(
    mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
    content::RenderFrameHost* rfh) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents)
    return;
  auto* print_manager = PrintViewManagerBasic::FromWebContents(web_contents);
  if (!print_manager)
    return;
  print_manager->BindReceiver(std::move(receiver), rfh);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PrintViewManagerBasic);

}  // namespace printing
