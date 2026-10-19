// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_handler.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_url_overrides.h"
#include "chrome/browser/ui/webui/new_tab_footer/mock_new_tab_footer_document.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/test/test_web_ui.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::SaveArg;

namespace {
const char kExtensionNtpName[] = "Extension-overridden NTP";
}

class NewTabFooterHandlerExtensionTest
    : public extensions::ExtensionServiceTestBase {
 public:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();

    InitializeEmptyExtensionService();
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    handler_ = std::make_unique<NewTabFooterHandler>(
        mojo::PendingReceiver<new_tab_footer::mojom::NewTabFooterHandler>(),
        document_.BindAndGetRemote(),
        base::WeakPtr<TopChromeWebUIController::Embedder>(),
        web_contents_.get());
    testing::Mock::VerifyAndClearExpectations(&document_);
  }

  void TearDown() override {
    testing::Mock::VerifyAndClearExpectations(&document_);
    handler_.reset();
    web_contents_.reset();
    ExtensionServiceTestBase::TearDown();
  }

  scoped_refptr<const extensions::Extension> LoadNtpExtension() {
    extensions::TestExtensionDir extension_dir;
    const std::string kManifest = R"(
      {
        "chrome_url_overrides": {
            "newtab": "ext.html"
        },
        "name": ")" + std::string(kExtensionNtpName) +
                                  R"(",
          "manifest_version": 3,
          "version": "0.1"
      })";
    extension_dir.WriteManifest(kManifest);
    extension_dir.WriteFile(
        FILE_PATH_LITERAL("ext.html"),
        std::string("<body>") + kExtensionNtpName + "</body>");
    extensions::ChromeTestExtensionLoader extension_loader(profile());
    scoped_refptr<const extensions::Extension> extension =
        extension_loader.LoadExtension(extension_dir.Pack());
    return extension;
  }

  void UnloadExtension(std::string extension_id) {
    extensions::ExtensionRegistrar::Get(profile())->RemoveExtension(
        extension_id, extensions::UnloadedExtensionReason::DISABLE);
  }

  NewTabFooterHandler& handler() { return *handler_; }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NewTabFooterHandler> handler_;
  testing::NiceMock<MockNewTabFooterDocument> document_;
};

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_NoExtension) {
  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });
  handler().UpdateNtpExtensionName();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ManualUpdate) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, kExtensionNtpName); });
  handler().UpdateNtpExtensionName();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ReadyExtension) {
  std::string name = "will change";
  EXPECT_CALL(document_, SetNtpExtensionName)
      .Times(2)
      .WillRepeatedly(
          [&name](const std::string& name_arg) { name = name_arg; });

  // Load the NTP extension and verify it's enabled.
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  document_.FlushForTesting();

  // Although OnExtensionReady was triggered, the footer isn't aware of the
  // extension until the URL override is activated.
  EXPECT_EQ(name, std::string());

  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  registry()->TriggerOnReady(extension.get());
  document_.FlushForTesting();

  EXPECT_EQ(name, kExtensionNtpName);
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_UnloadExtension) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  const std::string extension_id = extension->id();
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension_id));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });

  extensions::TestExtensionRegistryObserver observer(registry(), extension_id);
  UnloadExtension(extension_id);
  observer.WaitForExtensionUnloaded();

  document_.FlushForTesting();
}

TEST_F(NewTabFooterHandlerExtensionTest, AttachedTabStateUpdated) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));
  registry()->TriggerOnReady(extension.get());
  document_.FlushForTesting();

  new_tab_footer::mojom::NewTabPageType ntp_type;
  EXPECT_CALL(document_, AttachedTabStateUpdated)
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&ntp_type));

  handler().AttachedTabStateUpdated(GURL(extension->url()));
  document_.FlushForTesting();
  EXPECT_EQ(ntp_type, new_tab_footer::mojom::NewTabPageType::kExtension);

  handler().AttachedTabStateUpdated(
      GURL(chrome::kChromeUINewTabPageThirdPartyHost));
  document_.FlushForTesting();
  EXPECT_EQ(ntp_type, new_tab_footer::mojom::NewTabPageType::kOther);

  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_DisableByPolicy) {
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, std::string()); });

  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}

TEST_F(NewTabFooterHandlerExtensionTest, SetNtpExtensionName_ReenablePolicy) {
  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, false);
  auto extension = LoadNtpExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registrar()->IsExtensionEnabled(extension->id()));
  // Force activation of the URL override. The usual observer for
  // extension load isn't created in the unit test.
  ExtensionUrlOverrides::RegisterOrActivateChromeURLOverrides(
      profile(),
      extensions::URLOverrides::GetChromeURLOverrides(extension.get()));

  EXPECT_CALL(document_, SetNtpExtensionName)
      .WillOnce(
          [](const std::string& name) { EXPECT_EQ(name, kExtensionNtpName); });

  profile()->GetPrefs()->SetBoolean(
      prefs::kNTPFooterExtensionAttributionEnabled, true);

  document_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&document_);
}
