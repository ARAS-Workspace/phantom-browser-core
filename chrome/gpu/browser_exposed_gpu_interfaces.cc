// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/gpu/browser_exposed_gpu_interfaces.h"

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/gpu/chrome_content_gpu_client.h"
#include "media/media_buildflags.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace {

}  // namespace

void ExposeChromeGpuInterfacesToBrowser(
    ChromeContentGpuClient* client,
    viz::GpuServiceImpl* gpu_service,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    mojo::BinderMap* binders) {
}
