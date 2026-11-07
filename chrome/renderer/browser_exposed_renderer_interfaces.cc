// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/browser_exposed_renderer_interfaces.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "mojo/public/cpp/bindings/binder_map.h"

void ExposeChromeRendererInterfacesToBrowser(
    ChromeContentRendererClient* client,
    mojo::BinderMap* binders) {
  binders->Add<visitedlink::mojom::VisitedLinkNotificationSink>(
      client->GetChromeObserver()->visited_link_reader()->GetBindCallback(),
      base::SequencedTaskRunner::GetCurrentDefault());

  binders->Add<web_cache::mojom::WebCache>(
      base::BindRepeating(&web_cache::WebCacheImpl::BindReceiver,
                          base::Unretained(client->GetWebCache())),
      base::SequencedTaskRunner::GetCurrentDefault());
}
