// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views_content_client/views_content_client.h"

#include <utility>

#include "build/build_config.h"
#include "content/public/app/content_main.h"
#include "ui/views_content_client/views_content_main_delegate.h"

namespace ui {

ViewsContentClient::ViewsContentClient(int argc, const char** argv)
    : argc_(argc), argv_(argv) {
}

ViewsContentClient::~ViewsContentClient() {
}

int ViewsContentClient::RunMain() {
  ViewsContentMainDelegate delegate(this);
  content::ContentMainParams params(&delegate);

  params.argc = argc_;
  params.argv = argv_;

  return content::ContentMain(std::move(params));
}

void ViewsContentClient::OnPreMainMessageLoopRun(
    content::BrowserContext* browser_context,
    gfx::NativeWindow window_context) {
  std::move(on_pre_main_message_loop_run_callback_)
      .Run(browser_context, window_context);
}

void ViewsContentClient::OnResourcesLoaded() {
  if (on_resources_loaded_callback_)
    std::move(on_resources_loaded_callback_).Run();
}

}  // namespace ui
