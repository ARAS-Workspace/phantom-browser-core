// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"

#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/omnibox/omnibox_pedal_implementations.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/actions/history_clusters_action.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal.h"
#include "components/omnibox/browser/actions/tab_switch_action.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/mock_autocomplete_provider_client.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/omnibox/browser/search_provider.h"
#include "components/omnibox/browser/suggestion_answer.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_starter_pack_data.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_focus_type.pb.h"
#include "third_party/omnibox_proto/answer_type.pb.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/vector_icon_types.h"
#include "url/gurl.h"

namespace {
class RealboxHandlerPublic : public RealboxHandler {
 public:
  using RealboxHandler::RealboxHandler;
  using SearchboxHandler::autocomplete_controller_observation_;
  using SearchboxHandler::client;
  using SearchboxHandler::omnibox_controller;
  using SearchboxHandler::SetAutocompleteControllerForTesting;
};
}  // namespace

class RealboxHandlerTest : public InProcessBrowserTest,
                           public testing::WithParamInterface<bool> {
 public:
  RealboxHandlerTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features*/ {omnibox::internal::kWebUIOmniboxPopup},
        /*disabled_features*/ {});
  }

  RealboxHandlerTest(const RealboxHandlerTest&) = delete;
  RealboxHandlerTest& operator=(const RealboxHandlerTest&) = delete;
  ~RealboxHandlerTest() override = default;

 protected:
  testing::NiceMock<MockSearchboxPage> page_;
  std::unique_ptr<RealboxHandlerPublic> handler_;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    handler_ = std::make_unique<RealboxHandlerPublic>(
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        page_.BindAndGetRemote(), browser()->GetProfile(),
        /*web_contents=*/browser()->tab_strip_model()->GetActiveWebContents());
  }

  void TearDownOnMainThread() override { handler_.reset(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(RealboxHandlerTest, RealboxUpdatesEditModelInput) {
  // Stop observing the AutocompleteController instance which will be destroyed.
  handler_->autocomplete_controller_observation_.Reset();

  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  auto client = std::make_unique<MockAutocompleteProviderClient>();
  client->set_template_url_service(template_url_service);
  // Set a mock AutocompleteController.
  auto autocomplete_controller =
      std::make_unique<testing::NiceMock<MockAutocompleteController>>(
          std::move(client), AutocompleteClassifier::DefaultOmniboxProviders());
  raw_ptr<testing::NiceMock<MockAutocompleteController>>
      autocomplete_controller_ = autocomplete_controller.get();
  handler_->omnibox_controller()->SetAutocompleteControllerForTesting(
      std::move(autocomplete_controller));
  // Set a mock OmniboxEditModel.
  auto omnibox_edit_model =
      std::make_unique<testing::NiceMock<MockOmniboxEditModel>>(
          handler_->omnibox_controller());
  raw_ptr<testing::NiceMock<MockOmniboxEditModel>> omnibox_edit_model_ =
      omnibox_edit_model.get();
  handler_->omnibox_controller()->SetEditModelForTesting(
      std::move(omnibox_edit_model));

  ACMatches matches;
  for (size_t i = 0; i < 4; ++i) {
    AutocompleteMatch match(nullptr, 1000, false,
                            AutocompleteMatchType::URL_WHAT_YOU_TYPED);
    match.keyword = u"match";
    match.allowed_to_be_default_match = true;
    matches.push_back(match);
  }
  const GURL url = GURL("http://kong-foo.com");
  matches[2].destination_url = url;
  matches[2].provider = handler_->omnibox_controller()
                            ->autocomplete_controller()
                            ->search_provider();
  AutocompleteResult* result = &handler_->omnibox_controller()
                                    ->autocomplete_controller()
                                    ->published_result_;
  result->AppendMatches(matches);

  AutocompleteInput input;
  EXPECT_CALL(*autocomplete_controller_, Start(_))
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&input));

  handler_->QueryAutocomplete(
      0, u"", /*prevent_inline_autocomplete=*/false, 0,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/true, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  EXPECT_EQ(input.focus_type(), metrics::OmniboxFocusType::INTERACTION_FOCUS);

  auto modifiers = searchbox::mojom::ActionModifiers::New();
  handler_->OpenAutocompleteMatch(2, url, /*are_matches_showing=*/true,
                                  /*mouse_button=*/1, std::move(modifiers),
                                  /*via_keyboard=*/false);

  // Assert that the input gets correctly updated for the realbox.
  EXPECT_TRUE(omnibox_edit_model_->GetInputForTesting().IsZeroSuggest());
  EXPECT_EQ(u"", omnibox_edit_model_->GetInputForTesting().text());

  handler_->QueryAutocomplete(
      0, u"match", /*prevent_inline_autocomplete=*/false, 0,
      omnibox::SuggestInventory::SUGGEST_INVENTORY_DEFAULT,
      /*is_on_focus=*/false, /*keyword=*/"",
      searchbox::mojom::InputMethod::kKeyboard);

  // Assert that the input text gets correctly updated for the realbox.
  EXPECT_EQ(u"match", omnibox_edit_model_->GetInputForTesting().text());

  testing::Mock::VerifyAndClearExpectations(omnibox_edit_model_);
  testing::Mock::VerifyAndClearExpectations(autocomplete_controller_);
}

INSTANTIATE_TEST_SUITE_P(All, RealboxHandlerTest, testing::Bool());

// Tests that all Omnibox Pedal vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(RealboxHandlerTest, PedalVectorIcons) {
  std::unordered_map<OmniboxPedalId, scoped_refptr<OmniboxPedal>> pedals =
      GetPedalImplementations(/*incognito=*/true, /*guest=*/false,
                              /*testing=*/true);
  for (auto const& it : pedals) {
    const scoped_refptr<OmniboxPedal> pedal = it.second;
    const gfx::VectorIcon& vector_icon = pedal->GetVectorIcon();
    const std::string& svg_name =
        handler_->AutocompleteIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}

// Tests that all Omnibox Action vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(RealboxHandlerTest, ActionVectorIcons) {
  std::vector<scoped_refptr<OmniboxAction>> actions = {
      base::MakeRefCounted<history_clusters::HistoryClustersAction>(
          "test", history::ClusterKeywordData()),
      base::MakeRefCounted<TabSwitchAction>(GURL("test")),
  };
  for (auto const& action : actions) {
    const gfx::VectorIcon& vector_icon = action->GetVectorIcon();
    const std::string& svg_name =
        handler_->AutocompleteIconToResourceName(vector_icon);
    EXPECT_FALSE(svg_name.empty());
  }
}

// Tests that all Omnibox match vector icons map to an equivalent SVG for use in
// the NTP Realbox.
IN_PROC_BROWSER_TEST_P(RealboxHandlerTest, MatchVectorIcons) {
  for (int type = AutocompleteMatchType::URL_WHAT_YOU_TYPED;
       type != AutocompleteMatchType::NUM_TYPES; type++) {
    AutocompleteMatch match;
    match.type = static_cast<AutocompleteMatchType::Type>(type);
    if (match.type == AutocompleteMatchType::STARTER_PACK) {
      // All STARTER_PACK suggestions should have non-empty vector icons.
      for (int starter_pack_id = static_cast<int>(
               template_url_starter_pack_data::StarterPackId::kBookmarks);
           starter_pack_id !=
           static_cast<int>(template_url_starter_pack_data::StarterPackId::
                                kMaxStarterPackId);
           starter_pack_id++) {
        TemplateURLData turl_data;
        turl_data.starter_pack_id = starter_pack_id;
        TemplateURL turl(turl_data);
        const gfx::VectorIcon& vector_icon =
            match.GetVectorIcon(/*is_bookmark=*/false, &turl);
        const std::string& svg_name =
            handler_->AutocompleteIconToResourceName(vector_icon);
        EXPECT_FALSE(svg_name.empty());
      }
    } else {
      const bool is_bookmark = RealboxHandlerTest::GetParam();
      const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
      const std::string& svg_name =
          handler_->AutocompleteIconToResourceName(vector_icon);
      if (vector_icon.is_empty()) {
        // An empty resource name is effectively a blank icon.
        EXPECT_TRUE(svg_name.empty());
      } else if (is_bookmark) {
        EXPECT_EQ(
            features::IsWebUIRoundedIconsEnabled()
                ? "//resources/cr_components/searchbox/icons/bookmark_cr23.svg"
                : "//resources/cr_components/searchbox/icons/"
                  "bookmark_cr23_old.svg",
            svg_name);
      } else {
        EXPECT_FALSE(svg_name.empty());
      }
    }
  }
}

// Tests that all Omnibox Answer vector icons map to an equivalent SVG for use
// in the NTP Realbox.
IN_PROC_BROWSER_TEST_P(RealboxHandlerTest, AnswerVectorIcons) {
  for (int answer_type = omnibox::ANSWER_TYPE_DICTIONARY;
       answer_type != omnibox::AnswerType_ARRAYSIZE; answer_type++) {
    AutocompleteMatch match;
    match.answer_type = static_cast<omnibox::AnswerType>(answer_type);
    const bool is_bookmark = RealboxHandlerTest::GetParam();
    const gfx::VectorIcon& vector_icon = match.GetVectorIcon(is_bookmark);
    const std::string& svg_name =
        handler_->AutocompleteIconToResourceName(vector_icon);
    if (is_bookmark) {
      EXPECT_EQ(
          features::IsWebUIRoundedIconsEnabled()
              ? "//resources/cr_components/searchbox/icons/bookmark_cr23.svg"
              : "//resources/cr_components/searchbox/icons/"
                "bookmark_cr23_old.svg",
          svg_name);
    } else {
      EXPECT_FALSE(svg_name.empty());
      EXPECT_NE(
          features::IsWebUIRoundedIconsEnabled()
              ? "//resources/cr_components/searchbox/icons/search_cr23.svg"
              : "//resources/cr_components/searchbox/icons/"
                "search_cr23_old.svg",
          svg_name);
    }
  }
}
