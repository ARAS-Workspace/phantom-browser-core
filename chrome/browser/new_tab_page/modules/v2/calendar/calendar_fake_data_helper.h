// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_CALENDAR_FAKE_DATA_HELPER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_CALENDAR_FAKE_DATA_HELPER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/calendar_data.mojom.h"

namespace calendar::calendar_fake_data_helper {

using GetResponseCallback = base::OnceCallback<void(std::string)>;

std::vector<ntp::calendar::mojom::CalendarEventPtr> GetFakeEvents(
    bool has_attachments_enabled);

std::unique_ptr<std::string> GetFakeJsonResponse();

}  // namespace calendar::calendar_fake_data_helper

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_CALENDAR_CALENDAR_FAKE_DATA_HELPER_H_
