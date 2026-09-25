// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/group_configurations.h"

#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/configuration.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/group_constants.h"

namespace feature_engagement {

std::optional<GroupConfig> GetClientSideGroupConfig(
    const base::Feature* group) {

#if BUILDFLAG(IS_ANDROID)
  if (kClankDefaultBrowserPromosGroup.name == group->name) {
    // Default browser promos in this groups can only be shown once every seven
    // days.
    GroupConfig config = GroupConfig();
    config.valid = true;
    config.session_rate = Comparator(EQUAL, 0);
    config.trigger = EventConfig("default_browser_promos_group_trigger",
                                 Comparator(EQUAL, 0), 7, kMaxStoragePeriod);
    // Default Browser promos in this groups can be shown only if the Role
    // Manager promo is not shown in the 7 days period.
    config.event_configs.insert(
        EventConfig("role_manager_default_browser_promos_shown",
                    Comparator(EQUAL, 0), 7, kMaxStoragePeriod));
    return config;
  }
#endif  // BUILDFLAG(IS_ANDROID)

  if (kIPHDummyGroup.name == group->name) {
    // Only used for tests. Various magic tricks are used below to ensure this
    // config is invalid and unusable.
    GroupConfig config = GroupConfig();
    config.valid = true;
    config.session_rate = Comparator(LESS_THAN, 0);
    config.trigger =
        EventConfig("dummy_group_iph_trigger", Comparator(LESS_THAN, 0), 1, 1);
    return config;
  }

  return std::nullopt;
}

}  // namespace feature_engagement
