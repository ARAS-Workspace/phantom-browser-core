// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/chrome_omnibox_navigation_observer.h"

#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_utils.h"

class ChromeOmniboxNavigationObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ChromeOmniboxNavigationObserverTest(
      const ChromeOmniboxNavigationObserverTest&) = delete;
  ChromeOmniboxNavigationObserverTest& operator=(
      const ChromeOmniboxNavigationObserverTest&) = delete;

 protected:
  ChromeOmniboxNavigationObserverTest() = default;
  ~ChromeOmniboxNavigationObserverTest() override = default;

  content::NavigationController* navigation_controller() {
    return &(web_contents()->GetController());
  }

  TemplateURLService* model() {
    return TemplateURLServiceFactory::GetForProfile(profile());
  }

  // Functions that return the name of certain search keywords that are part
  // of the TemplateURLService attached to this profile.
  static std::u16string auto_generated_search_keyword() {
    return u"auto_generated_search_keyword";
  }
  static std::u16string non_auto_generated_search_keyword() {
    return u"non_auto_generated_search_keyword";
  }
  static std::u16string default_search_keyword() {
    return u"default_search_keyword";
  }
  static std::u16string prepopulated_search_keyword() {
    return u"prepopulated_search_keyword";
  }
  static std::u16string policy_search_keyword() {
    return u"policy_search_keyword";
  }
  static std::u16string starter_pack_keyword() {
    return u"starter_pack_keyword";
  }

 private:
  // ChromeRenderViewHostTestHarness:
  void SetUp() override;
};

void ChromeOmniboxNavigationObserverTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Set up a series of search engines for later testing.
  TemplateURLServiceFactoryTestUtil factory_util(profile());
  factory_util.VerifyLoad();

  TemplateURLData auto_gen_turl;
  auto_gen_turl.SetKeyword(auto_generated_search_keyword());
  auto_gen_turl.safe_for_autoreplace = true;
  factory_util.model()->Add(std::make_unique<TemplateURL>(auto_gen_turl));

  TemplateURLData non_auto_gen_turl;
  non_auto_gen_turl.SetKeyword(non_auto_generated_search_keyword());
  factory_util.model()->Add(std::make_unique<TemplateURL>(non_auto_gen_turl));

  TemplateURLData default_turl;
  default_turl.SetKeyword(default_search_keyword());
  factory_util.model()->SetUserSelectedDefaultSearchProvider(
      factory_util.model()->Add(std::make_unique<TemplateURL>(default_turl)));

  TemplateURLData prepopulated_turl;
  prepopulated_turl.SetKeyword(prepopulated_search_keyword());
  prepopulated_turl.prepopulate_id = 1;
  factory_util.model()->Add(std::make_unique<TemplateURL>(prepopulated_turl));

  TemplateURLData policy_turl;
  policy_turl.SetKeyword(policy_search_keyword());
  policy_turl.policy_origin =
      TemplateURLData::PolicyOrigin::kDefaultSearchProvider;
  factory_util.model()->Add(std::make_unique<TemplateURL>(policy_turl));

  TemplateURLData starter_pack_turl;
  starter_pack_turl.SetKeyword(starter_pack_keyword());
  starter_pack_turl.starter_pack_id = 1;
  factory_util.model()->Add(std::make_unique<TemplateURL>(starter_pack_turl));
}

namespace {

scoped_refptr<net::HttpResponseHeaders> GetHeadersForResponseCode(int code) {
  if (code == 200) {
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 200 OK\r\n");
  } else if (code == 404) {
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 404 Not Found\r\n");
  }
  NOTREACHED();
}

void WriteMojoMessage(const mojo::ScopedDataPipeProducerHandle& handle,
                      std::string message) {
  size_t actually_written_bytes = 0;
  ASSERT_EQ(MOJO_RESULT_OK, handle->WriteData(base::as_byte_span(message),
                                              MOJO_WRITE_DATA_FLAG_NONE,
                                              actually_written_bytes));
  ASSERT_EQ(message.size(), actually_written_bytes);
}

}  // namespace

TEST_F(ChromeOmniboxNavigationObserverTest, DeleteBrokenCustomSearchEngines) {
  // The actual URL doesn't matter for this test as long as it's valid.
  struct TestData {
    std::u16string keyword;
    int status_code;
    bool expect_exists;
  };
  std::vector<TestData> cases = {
      {auto_generated_search_keyword(), 200, true},
      {auto_generated_search_keyword(), 404, false},
      {non_auto_generated_search_keyword(), 404, true},
      {default_search_keyword(), 404, true},
      {prepopulated_search_keyword(), 404, true},
      {policy_search_keyword(), 404, true},
      {starter_pack_keyword(), 404, true}};

  std::u16string query = u" text";
  for (size_t i = 0; i < cases.size(); ++i) {
    SCOPED_TRACE("case #" + base::NumberToString(i));
    // The keyword should always exist at the beginning.
    EXPECT_TRUE(model()->GetTemplateURLForKeyword(cases[i].keyword));

    AutocompleteMatch match;
    match.keyword = cases[i].keyword;
    // Append the case number to the URL to ensure that we are loading a new
    // page.
    auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
        GURL(base::StringPrintf("https://foo.com/%zu", i)), web_contents());
    navigation->SetResponseHeaders(
        GetHeadersForResponseCode(cases[i].status_code));

    // HTTPErrorNavigationThrottle checks whether body is null or not to decide
    // whether to commit an error page or a regular one.
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    ASSERT_EQ(mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle),
              MOJO_RESULT_OK);
    navigation->SetResponseBody(std::move(consumer_handle));
    WriteMojoMessage(producer_handle, "data");

    navigation->Start();
    ChromeOmniboxNavigationObserver::Create(navigation->GetNavigationHandle(),
                                            profile(), cases[i].keyword + query,
                                            match);

    navigation->Commit();

    EXPECT_EQ(cases[i].expect_exists,
              model()->GetTemplateURLForKeyword(cases[i].keyword) != nullptr);
  }
}
