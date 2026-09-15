// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace language_detection::features {

// If enabled, we lazily initiate `TranslateAgent` in
// `ChromeRenderFrameObserver` (crbug/361215212).
BASE_FEATURE(kLazyUpdateTranslateModel, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsTFLiteLanguageDetectionEnabled() {
// The feature is explicitly disabled on WebView.
// TODO(crbug.com/40819484): Enable the feature on WebView.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)
  return true;
#else
  return false;
#endif
}

}  // namespace language_detection::features
