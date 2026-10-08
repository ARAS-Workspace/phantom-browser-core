// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/permission_bubble/permission_bubble_browser_test_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_request.h"

PermissionBubbleBrowserTest::PermissionBubbleBrowserTest() = default;

PermissionBubbleBrowserTest::~PermissionBubbleBrowserTest() = default;

void PermissionBubbleBrowserTest::SetUpOnMainThread() {
  ExtensionBrowserTest::SetUpOnMainThread();

  // // Add a single permission request.
  std::vector<std::unique_ptr<permissions::PermissionRequest>> requests;
  requests.push_back(std::make_unique<permissions::MockPermissionRequest>(
      permissions::RequestType::kNotifications, /*request_state=*/nullptr));

  test_delegate_.set_requests(std::move(requests));
}

PermissionBubbleKioskBrowserTest::PermissionBubbleKioskBrowserTest() = default;

PermissionBubbleKioskBrowserTest::~PermissionBubbleKioskBrowserTest() = default;

void PermissionBubbleKioskBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PermissionBubbleBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(switches::kKioskMode);
  // Navigate to a test file URL.
  GURL test_file_url(chrome_test_utils::GetTestUrl(
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("simple.html"))));
  command_line->AppendArg(test_file_url.spec());
}
