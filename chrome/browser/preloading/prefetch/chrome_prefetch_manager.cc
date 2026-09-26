// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/chrome_prefetch_manager.h"

#include "chrome/browser/preloading/chrome_preloading.h"
#include "content/public/browser/preload_pipeline_info.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/common/content_features.h"
#include "net/http/http_no_vary_search_data.h"
#include "third_party/blink/public/mojom/loader/referrer.mojom.h"

ChromePrefetchManager::~ChromePrefetchManager() = default;

ChromePrefetchManager::ChromePrefetchManager(content::WebContents* web_contents)
    : content::WebContentsUserData<ChromePrefetchManager>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ChromePrefetchManager);
