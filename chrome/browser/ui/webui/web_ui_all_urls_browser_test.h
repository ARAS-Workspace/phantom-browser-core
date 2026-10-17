// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

// Class which performs necessary setup to load all chrome urls. If a URL
// requires a feature flag or other special setup to load correctly, add
// this setup here.
class WebUIAllUrlsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<std::string_view> {
 public:
  WebUIAllUrlsBrowserTest();
  WebUIAllUrlsBrowserTest(const WebUIAllUrlsBrowserTest&) = delete;
  WebUIAllUrlsBrowserTest& operator=(const WebUIAllUrlsBrowserTest&) = delete;
  ~WebUIAllUrlsBrowserTest() override;
  static std::string ParamInfoToString(
      const testing::TestParamInfo<std::string_view>& info);

 protected:
  void WaitBeforeNavigation();

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_
