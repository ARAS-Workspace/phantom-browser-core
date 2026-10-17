// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_

#include <array>

namespace ntp_modules {

inline constexpr char kAllModulesId[] = "all";
inline constexpr char kDummyModuleId[] = "dummy";
inline constexpr char kMostRelevantTabResumptionModuleId[] = "tab_resumption";
inline constexpr char kTabGroupsModuleId[] = "tab_groups";

// Default order for modules.
inline constexpr std::array<const char*, 1> kOrderedModuleIds = {
    ntp_modules::kMostRelevantTabResumptionModuleId};

}  // namespace ntp_modules

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_MODULES_CONSTANTS_H_
