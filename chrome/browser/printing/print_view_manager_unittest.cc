// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"

#include <memory>
#include <utility>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using PrintViewManagerTest = PrintPreviewTest;

namespace {

class TestPrintViewManagerForSystemDialogPrint : public PrintViewManager {
 public:
  explicit TestPrintViewManagerForSystemDialogPrint(
      content::WebContents* web_contents)
      : PrintViewManager(web_contents) {}
  ~TestPrintViewManagerForSystemDialogPrint() override = default;

  // PrintViewManager:
  void PrintForSystemDialogImpl() override {
    // There has to be a target frame so DidShowPrintDialog() does not crash.
    // Manually set it, as there is no IPC in progress.
    print_manager_host_receivers_for_testing().SetCurrentTargetFrameForTesting(
        web_contents()->GetPrimaryMainFrame());
    DidShowPrintDialog();
    print_manager_host_receivers_for_testing().SetCurrentTargetFrameForTesting(
        nullptr);
  }
};

}  // namespace

TEST_F(PrintViewManagerTest, PrintSubFrameAndDestroy) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* sub_frame =
      content::RenderFrameHostTester::For(web_contents->GetPrimaryMainFrame())
          ->AppendChild("child");

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(web_contents);
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->print_preview_rfh());

  print_view_manager->PrintPreviewNow(sub_frame, false);
  EXPECT_TRUE(print_view_manager->print_preview_rfh());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->print_preview_rfh());
}

TEST_F(PrintViewManagerTest, PrintForSystemDialog) {
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  auto print_view_manager =
      std::make_unique<TestPrintViewManagerForSystemDialogPrint>(web_contents);

  ASSERT_TRUE(print_view_manager->PrintPreviewNow(
      web_contents->GetPrimaryMainFrame(), /*has_selection=*/false));

  base::RunLoop run_loop;
  bool dialog_shown = false;
  EXPECT_TRUE(print_view_manager->PrintForSystemDialogNow(
      base::BindLambdaForTesting([&]() {
        dialog_shown = true;
        run_loop.Quit();
      })));
  run_loop.Run();
  EXPECT_TRUE(dialog_shown);

  print_view_manager->PrintPreviewDone();
}

}  // namespace printing
