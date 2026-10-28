// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/test_autofill_manager_waiter.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
namespace {

// Adds waiting capabilities to BrowserAutofillManager.
class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(ContentAutofillDriver* driver)
      : BrowserAutofillManager(driver) {}

  static TestAutofillManager& GetForRenderFrameHost(
      content::RenderFrameHost* rfh) {
    return static_cast<TestAutofillManager&>(
        ContentAutofillDriver::GetForRenderFrameHost(rfh)
            ->GetAutofillManager());
  }

  [[nodiscard]] testing::AssertionResult WaitForFormsSeen(
      size_t num_awaited_calls) {
    return forms_seen_.Wait(num_awaited_calls);
  }

 private:
  TestAutofillManagerWaiter forms_seen_{*this,
                                        {AutofillManagerEvent::kFormsSeen}};
};

class DevToolsAutofillTest : public DevToolsProtocolTestBase {
 public:
  DevToolsAutofillTest() = default;
  void SetUpCommandLine(base::CommandLine* command_line) override {
    DevToolsProtocolTestBase::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  content::RenderFrameHost* main_frame() {
    return web_contents()->GetPrimaryMainFrame();
  }

  TestAutofillManager& main_autofill_manager() {
    return TestAutofillManager::GetForRenderFrameHost(main_frame());
  }

  base::DictValue GetTestCreditCard() {
    base::DictValue card;
    card.Set("number", "4444444444444444");
    card.Set("name", "John Smith");
    card.Set("expiryMonth", "01");
    card.Set("expiryYear", "2030");
    card.Set("cvc", "123");
    return card;
  }

 private:
  test::AutofillBrowserTestEnvironment autofill_test_environment_;
  TestAutofillManagerInjector<TestAutofillManager> autofill_manager_injector_;
};

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, SetAddresses) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/autofill_creditcard_form.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  ASSERT_TRUE(content::WaitForLoadStop(web_contents()));
  Attach();

  EXPECT_TRUE(main_autofill_manager().WaitForFormsSeen(1));

  base::DictValue address_1_fields;
  address_1_fields.Set("name", "ADDRESS_HOME_LINE1");
  address_1_fields.Set("value", "Erika-mann");
  base::DictValue address_1;
  base::ListValue fields;
  fields.Append(std::move(address_1_fields));
  address_1.Set("fields", std::move(fields));

  base::DictValue address_2_fields;
  address_2_fields.Set("name", "ADDRESS_HOME_LINE2");
  address_2_fields.Set("value", "Faria lima");
  base::DictValue address_2;
  base::ListValue fields_2;
  fields_2.Append(std::move(address_2_fields));
  address_2.Set("fields", std::move(fields_2));

  base::ListValue test_addresses;
  test_addresses.Append(std::move(address_1));
  test_addresses.Append(std::move(address_2));

  base::DictValue params;
  params.Set("addresses", std::move(test_addresses));

  SendCommandSync("Autofill.setAddresses", std::move(params));

  base::span<const AutofillProfile> res =
      main_autofill_manager().client().GetTestAddresses();
  ASSERT_EQ(res.size(), 2u);
  ASSERT_EQ(res[0].GetAddress().GetRawInfo(FieldType::ADDRESS_HOME_LINE1),
            u"Erika-mann");
  ASSERT_EQ(res[1].GetAddress().GetRawInfo(FieldType::ADDRESS_HOME_LINE2),
            u"Faria lima");
}

IN_PROC_BROWSER_TEST_F(DevToolsAutofillTest, TriggerWithInvalidFieldId) {
  embedded_test_server()->ServeFilesFromSourceDirectory(
      "chrome/test/data/autofill");
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url(
      embedded_test_server()->GetURL("/autofill_creditcard_form.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));

  Attach();
  SendCommandSync("Autofill.enable");

  EXPECT_TRUE(main_autofill_manager().WaitForFormsSeen(1));

  base::DictValue params;
  params.Set("fieldId", 99999);  // Invalid field ID
  params.Set("card", GetTestCreditCard());

  SendCommandSync("Autofill.trigger", std::move(params));

  EXPECT_TRUE(error());
  EXPECT_EQ(*error()->FindString("message"), "Field not found");
}

}  // namespace
}  // namespace autofill
