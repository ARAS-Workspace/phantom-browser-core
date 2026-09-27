// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_PLATFORM_BROWSER_TEST_H_
#define CHROME_TEST_BASE_PLATFORM_BROWSER_TEST_H_

#include "build/build_config.h"

#include "chrome/test/base/in_process_browser_test.h"

// PlatformBrowserTest is aliased to InProcessBrowserTest.
// Further details and methodology can be found in the design doc:
// https://docs.google.com/document/d/1jT3W6VnVI4b0FuiNbYzgGZPxIOUZmppUZZwi3OebvVE/preview
using PlatformBrowserTest = InProcessBrowserTest;

#endif  // CHROME_TEST_BASE_PLATFORM_BROWSER_TEST_H_
