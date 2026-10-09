// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chooser_controller/title_util.h"

#include "base/command_line.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"  // nogncheck
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

namespace {

constexpr int kTitleResourceId = IDS_USB_DEVICE_CHOOSER_PROMPT;

using CreateChooserTitleTest = ChromeRenderViewHostTestHarness;

TEST_F(CreateChooserTitleTest, NoFrame) {
  EXPECT_EQ(u"", CreateChooserTitle(nullptr, kTitleResourceId));
}

TEST_F(CreateChooserTitleTest, UrlFrameTree) {
  NavigateAndCommit(GURL("https://main-frame.com"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://sub-frame.com"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  EXPECT_EQ("main-frame.com", main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  EXPECT_EQ("sub-frame.com", subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"main-frame.com wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
TEST_F(CreateChooserTitleTest, ExtensionsFrameTree) {
  auto manifest = base::DictValue()
                      .Set("name", "Chooser Title Subframe Test")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("web_accessible_resources",
                           base::ListValue().Append("index.html"));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder().SetManifest(std::move(manifest)).Build();
  ASSERT_TRUE(extension);

  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));
  extension_system->CreateExtensionService(
      base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
  extensions::ExtensionRegistrar::Get(profile())->AddExtension(extension.get());

  NavigateAndCommit(extension->GetResourceURL("index.html"));
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("data:text/html,"),
          content::RenderFrameHostTester::For(main_rfh())
              ->AppendChild("subframe"));

  ASSERT_EQ(extension->id(), main_rfh()->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"Chooser Title Subframe Test wants to connect",
            CreateChooserTitle(main_rfh(), kTitleResourceId));
  ASSERT_NE(extension->id(), subframe->GetLastCommittedOrigin().host());
  EXPECT_EQ(u"Chooser Title Subframe Test wants to connect",
            CreateChooserTitle(subframe, kTitleResourceId));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS_CORE)

}  // namespace
