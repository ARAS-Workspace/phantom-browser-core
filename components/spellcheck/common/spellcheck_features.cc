// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/common/spellcheck_features.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck {

#if BUILDFLAG(ENABLE_SPELLCHECK)

bool UseBrowserSpellChecker() {
#if !BUILDFLAG(USE_BROWSER_SPELLCHECKER)
  return false;
#else
  return true;
#endif
}

#if BUILDFLAG(IS_ANDROID)
bool IsAndroidSpellCheckFeatureEnabled() {
  return !base::SysInfo::IsLowEndDevice();
}

BASE_FEATURE(kAndroidGrammarCheck, base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kEnableSpellcheckRegionalSignal,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kLazyInitializeSpellcheckCharAttribute,
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(ENABLE_SPELLCHECK)

}  // namespace spellcheck
