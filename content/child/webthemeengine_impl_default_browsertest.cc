// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace content {

class WebThemeEngineImplDefaultBrowserTest : public ContentBrowserTest {
 public:
  WebThemeEngineImplDefaultBrowserTest() = default;
};

IN_PROC_BROWSER_TEST_F(WebThemeEngineImplDefaultBrowserTest,
                       FieldAndCanvasAreDistinctInDarkMode) {
  GURL url(
      "data:text/html,"
      "<!doctype html><html>"
      "<body style='color-scheme: dark;'>"
      "<div id='field' style='color: Field'>Field</div>"
      "<div id='canvas' style='color: Canvas'>Canvas</div>"
      "</body></html>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  const std::string field_color =
      EvalJs(shell(),
             "window.getComputedStyle(document.getElementById('field'))."
             "getPropertyValue('color').toString()")
          .ExtractString();
  const std::string canvas_color =
      EvalJs(shell(),
             "window.getComputedStyle(document.getElementById('canvas'))."
             "getPropertyValue('color').toString()")
          .ExtractString();

  EXPECT_NE(field_color, canvas_color);
}

IN_PROC_BROWSER_TEST_F(WebThemeEngineImplDefaultBrowserTest,
                       ActiveLinkAndVisitedTextAreDistinctInDarkMode) {
  GURL url(
      "data:text/html,"
      "<!doctype html><html>"
      "<body style='color-scheme: dark;'>"
      "<div id='active-text' style='color: ActiveText'>ActiveText</div>"
      "<div id='link-text' style='color: LinkText'>LinkText</div>"
      "<div id='visited-text' style='color: VisitedText'>VisitedText</div>"
      "</body></html>");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  const std::string active_text_color =
      EvalJs(shell(),
             "window.getComputedStyle(document.getElementById('active-text'))."
             "getPropertyValue('color').toString()")
          .ExtractString();
  const std::string link_text_color =
      EvalJs(shell(),
             "window.getComputedStyle(document.getElementById('link-text'))."
             "getPropertyValue('color').toString()")
          .ExtractString();
  const std::string visitied_text_color =
      EvalJs(shell(),
             "window.getComputedStyle(document.getElementById('visited-text'))."
             "getPropertyValue('color').toString()")
          .ExtractString();

  EXPECT_NE(active_text_color, link_text_color);
  EXPECT_NE(link_text_color, visitied_text_color);
  EXPECT_NE(visitied_text_color, active_text_color);
}
}  // namespace content
