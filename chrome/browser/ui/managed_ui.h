// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_MANAGED_UI_H_
#define CHROME_BROWSER_UI_MANAGED_UI_H_

#include <optional>
#include <string>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

class GURL;
class Profile;

namespace gfx {
struct VectorIcon;
}

// Returns true if a 'Managed by <...>' message should appear in
// Chrome's App Menu, and on the following chrome:// pages:
// - chrome://bookmarks
// - chrome://downloads
// - chrome://extensions
// - chrome://history
// - chrome://settings
//
// This applies to all forms of management (eg. both Enterprise and Parental
// controls), a suitable string will be returned by the methods below.
//
// N.B.: This is independent of Chrome OS's system tray message for enterprise
// users.
bool ShouldDisplayManagedUi(Profile* profile);

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)
// The icon to use in the Managed UI.
const gfx::VectorIcon& GetManagedUiIcon(Profile* profile);
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(IS_ANDROID)
std::u16string GetManagementPageSubtitle(Profile* profile);
#endif

#endif  // CHROME_BROWSER_UI_MANAGED_UI_H_
