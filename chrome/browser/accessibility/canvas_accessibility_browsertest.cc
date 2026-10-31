// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

using CanvasAccessibilityTestParams = features::CanvasAccessibilityMode;

const ui::AXNodeData* FindFirstNodeWithRole(const ui::AXTreeUpdate& tree_update,
                                            ax::mojom::Role role) {
  for (const auto& node : tree_update.nodes) {
    if (node.role == role) {
      return &node;
    }
  }
  return nullptr;
}

class CanvasAccessibilityBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<CanvasAccessibilityTestParams> {
 public:
  CanvasAccessibilityBrowserTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    switch (GetCanvasAccessibilityMode()) {
      case features::CanvasAccessibilityMode::kDisabled:
        disabled_features.push_back(features::kAccessibilityCanvas);
        break;
      case features::CanvasAccessibilityMode::kBasic:
        enabled_features.push_back({features::kAccessibilityCanvas,
                                    {{"CanvasAccessibilityMode", "Basic"}}});
        break;
      case features::CanvasAccessibilityMode::kAdvanced:
        enabled_features.push_back({features::kAccessibilityCanvas,
                                    {{"CanvasAccessibilityMode", "Advanced"}}});
        break;
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }
  ~CanvasAccessibilityBrowserTest() override = default;

  features::CanvasAccessibilityMode GetCanvasAccessibilityMode() const {
    return GetParam();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());

    content::BrowserAccessibilityState::GetInstance()
        ->SetActivationFromPlatformEnabled(true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<content::ScopedAccessibilityModeOverride>
      scoped_accessibility_mode_;
};

IN_PROC_BROWSER_TEST_P(CanvasAccessibilityBrowserTest,
                       FillTextCanvasAnnotation) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui::AXMode mode = ui::kAXModeComplete;
  mode.set_mode(ui::AXMode::kScreenReader, true);
  scoped_accessibility_mode_ =
      std::make_unique<content::ScopedAccessibilityModeOverride>(web_contents,
                                                                 mode);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/accessibility/canvas_filltext.html")));

  bool expect_filltext_annotation =
      (GetCanvasAccessibilityMode() !=
       features::CanvasAccessibilityMode::kDisabled);

  if (expect_filltext_annotation) {
    content::WaitForAccessibilityTreeToContainNodeWithName(web_contents,
                                                           "Hello Canvas");
  } else {
    ui::AXTreeUpdate tree_update =
        content::GetAccessibilityTreeSnapshot(web_contents);
    const ui::AXNodeData* canvas_node =
        FindFirstNodeWithRole(tree_update, ax::mojom::Role::kCanvas);
    if (canvas_node) {
      std::string annotation = canvas_node->GetStringAttribute(
          ax::mojom::StringAttribute::kCanvasAnnotation);
      EXPECT_TRUE(annotation.empty());
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CanvasAccessibilityBrowserTest,
    ::testing::Values(features::CanvasAccessibilityMode::kDisabled,
                      features::CanvasAccessibilityMode::kBasic,
                      features::CanvasAccessibilityMode::kAdvanced),
    [](const ::testing::TestParamInfo<CanvasAccessibilityTestParams>& info) {
      std::string name;
      switch (info.param) {
        case features::CanvasAccessibilityMode::kDisabled:
          name += "Disabled";
          break;
        case features::CanvasAccessibilityMode::kBasic:
          name += "Basic";
          break;
        case features::CanvasAccessibilityMode::kAdvanced:
          name += "Advanced";
          break;
      }
      return name;
    });

}  // namespace
