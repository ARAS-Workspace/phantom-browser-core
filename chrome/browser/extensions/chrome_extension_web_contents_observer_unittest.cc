// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/content/browser/error_reporting/js_error_report_processor.h"
#include "components/crx_file/id_util.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class MockJsErrorReportProcessor : public JsErrorReportProcessor {
 public:
  void SendErrorReport(JavaScriptErrorReport error_report,
                       base::OnceClosure completion_callback,
                       content::BrowserContext* browser_context) override {
    last_report_ = std::move(error_report);
    std::move(completion_callback).Run();
  }

  void SetAsDefault() { JsErrorReportProcessor::SetDefault(this); }
  static void ResetDefault() { JsErrorReportProcessor::SetDefault(nullptr); }

  const JavaScriptErrorReport& last_report() const { return last_report_; }

 protected:
  ~MockJsErrorReportProcessor() override = default;

 private:
  JavaScriptErrorReport last_report_;
};

}  // namespace

class ChromeExtensionWebContentsObserverUnitTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeExtensionWebContentsObserverUnitTest() = default;
  ~ChromeExtensionWebContentsObserverUnitTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableCrashOnComponentExtensionJsError);

    TestExtensionSystem* extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()));
    extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ChromeExtensionWebContentsObserver::CreateForWebContents(web_contents());
  }

  void TearDown() override {
    MockJsErrorReportProcessor::ResetDefault();
    ExtensionRegistry::Get(profile())->ClearAll();
    ChromeRenderViewHostTestHarness::TearDown();
  }
};

TEST_F(ChromeExtensionWebContentsObserverUnitTest,
       NonComponentExtensionJavaScriptErrorsIgnored) {
  auto mock_processor = base::MakeRefCounted<MockJsErrorReportProcessor>();
  mock_processor->SetAsDefault();

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Normal Extension")
          .SetLocation(mojom::ManifestLocation::kExternalComponent)
          .SetID(crx_file::id_util::GenerateId("test_normal"))
          .Build();
  ExtensionRegistrar::Get(profile())->AddExtension(extension);

  GURL extension_url = extension->GetResourceURL("popup.html");
  NavigateAndCommit(extension_url);

  ChromeExtensionWebContentsObserver* observer =
      ChromeExtensionWebContentsObserver::FromWebContents(web_contents());
  ASSERT_TRUE(observer);

  static_cast<content::WebContentsObserver*>(observer)
      ->OnDidAddMessageToConsole(
          main_rfh(), blink::mojom::ConsoleMessageLevel::kError,
          u"Testing normal extension JS crash telemetry", 42,
          base::UTF8ToUTF16(extension_url.spec()), std::nullopt);

  const JavaScriptErrorReport& report = mock_processor->last_report();
  EXPECT_TRUE(report.message.empty());
}

}  // namespace extensions
