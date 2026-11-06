// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/features.h"

#include "base/feature_list.h"

namespace features {

// Whether an infobar is shown when the process is shared.
BASE_FEATURE(kDevToolsSharedProcessInfobar, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether showing animation styles in the styles tab is enabled.
BASE_FEATURE(kDevToolsAnimationStylesInStylesTab,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the DevTools AI generated annotation labels in timeline are enabled.
BASE_FEATURE(kDevToolsNewPermissionDialog, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether DevTools drawer can be toggled to vertical orientation.
BASE_FEATURE(kDevToolsVerticalDrawer, base::FEATURE_ENABLED_BY_DEFAULT);

// Whether Network panel should use Durable Messages to preserve network bodies.
BASE_FEATURE(kDevToolsEnableDurableMessages, base::FEATURE_ENABLED_BY_DEFAULT);

// If enabled, allows starting remote debugging in a running Chrome instance.
BASE_FEATURE(kDevToolsAcceptDebuggingConnections,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the policy dialog should be shown instead of greying out the
// Developer Tools toggle.
// TODO(crbug.com/442892562): Remove this flag once the feature is launched.
BASE_FEATURE(kDevToolsShowPolicyDialog, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDevToolsComments, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Protocol Monitor panel is enabled.
BASE_FEATURE(kDevToolsProtocolMonitor, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the Plus Button is shown in main DevTools panel/drawer tabbed
// locations, replacing the "More tools" overflow menu.
BASE_FEATURE(kDevToolsPlusButton, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether instrumentation breakpoints are enabled in DevTools.
BASE_FEATURE(kDevToolsInstrumentationBreakpoints,
             "DevToolsInstrumentationBreakpoints",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether source map scopes are enabled in the DevTools Sources panel.
BASE_FEATURE(kDevToolsSourceMapScopesInSourcesPanel,
             "DevToolsSourceMapScopesInSourcesPanel",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Whether the A11y Announcements recording subpane is enabled in DevTools.
BASE_FEATURE(kDevToolsAriaLiveRecording, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether mobile safe area emulation is enabled in DevTools.
BASE_FEATURE(kDevToolsMobileSafeAreaEmulation,
             base::FEATURE_DISABLED_BY_DEFAULT);
}  // namespace features
