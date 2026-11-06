// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_FEATURES_H_
#define CHROME_BROWSER_DEVTOOLS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

BASE_DECLARE_FEATURE(kDevToolsSharedProcessInfobar);

BASE_DECLARE_FEATURE(kDevToolsAnimationStylesInStylesTab);

BASE_DECLARE_FEATURE(kDevToolsNewPermissionDialog);

BASE_DECLARE_FEATURE(kDevToolsVerticalDrawer);

BASE_DECLARE_FEATURE(kDevToolsEnableDurableMessages);

BASE_DECLARE_FEATURE(kDevToolsAcceptDebuggingConnections);

BASE_DECLARE_FEATURE(kDevToolsShowPolicyDialog);

BASE_DECLARE_FEATURE(kDevToolsComments);

BASE_DECLARE_FEATURE(kDevToolsProtocolMonitor);

BASE_DECLARE_FEATURE(kDevToolsPlusButton);

BASE_DECLARE_FEATURE(kDevToolsInstrumentationBreakpoints);

BASE_DECLARE_FEATURE(kDevToolsSourceMapScopesInSourcesPanel);

BASE_DECLARE_FEATURE(kDevToolsAriaLiveRecording);

BASE_DECLARE_FEATURE(kDevToolsMobileSafeAreaEmulation);
}  // namespace features

#endif  // CHROME_BROWSER_DEVTOOLS_FEATURES_H_
