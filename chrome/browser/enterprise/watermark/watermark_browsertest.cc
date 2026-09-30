// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/browser/enterprise/data_protection/data_protection_navigation_controller.h"
#include "chrome/browser/enterprise/data_protection/data_protection_overlay_view.h"
#include "chrome/browser/enterprise/watermark/settings.h"
#include "chrome/browser/enterprise/watermark/watermark_features.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/test/test_browser_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/watermark/watermark_page_handler.h"
#include "chrome/browser/ui/webui/watermark/watermark_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/enterprise/data_controls/core/browser/test_utils.h"
#include "components/enterprise/data_protection/features.h"
#include "components/enterprise/data_protection/utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/browser/realtime/fake_url_lookup_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace enterprise_watermark {

namespace {

// This string checks that non-latin characters render correctly.
constexpr char kMultilingualWatermarkMessage[] = R"(
    THIS IS CONFIDENTIAL!

    😀😀😀 草草草 www

    مضحك جداً
)";

// This string checks dynamic block width: short and medium lines expand block
// width without splitting, while extremely long lines exceeding the maximum cap
// are split up into multiple lines.
constexpr char kLongLinesWatermarkMessage[] = R"(

This dynamically expands the block width and stays on one line
This is a short line
It is not split
This is another very long line that should be split up into multiple lines
)";

constexpr char kTimestampUtcWatermarkMessage[] =
    "Confidential (UTC)\nuser@example.com\n";
constexpr char kTimestampTokyoWatermarkMessage[] =
    "Confidential (Tokyo)\nuser@example.com\n";
constexpr char kTimestampTorontoWatermarkMessage[] =
    "Confidential (Toronto)\nuser@example.com\n";
constexpr char kTimestampKolkataWatermarkMessage[] =
    "Confidential (Kolkata)\nuser@example.com\n";

struct WatermarkTextParams {
  const char* test_suffix;
  const char* watermark_text;
  const char* timezone = nullptr;
};

struct WatermarkParams {
  const char* test_suffix;
  int fill_opacity;
  int outline_opacity;
  int font_size;
  const char* watermark_text;
};

constexpr SkColor kTestFillColor = SkColorSetARGB(0x2A, 0, 0, 0);
constexpr SkColor kTestOutlineColor = SkColorSetARGB(0x3D, 255, 255, 255);
constexpr int kTestFontSize =
    enterprise_connectors::kWatermarkStyleFontSizeDefault;

class WatermarkBrowserTest
    : public UiBrowserTest,
      public testing::WithParamInterface<WatermarkTextParams> {
 public:
  WatermarkBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_data_protection::kEnableWatermarkTimestampTimezone);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void NavigateToTestPage() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(
                       "/enterprise/watermark/watermark_test_page.html")));
  }

  // Returns true if a watermark view object was available to set the watermark.
  bool SetWatermark(const std::string& watermark_message) {
    std::string message = watermark_message;
    if (GetParam().timezone) {
      GetProfile()->GetPrefs()->SetString(
          enterprise_connectors::kWatermarkStyleTimestampTimezonePref,
          GetParam().timezone);

      std::string timestamp_timezone =
          GetTimestampTimezone(GetProfile()->GetPrefs());

      base::test::ScopedRestoreDefaultTimezone timezone("UTC");
      message += enterprise_data_protection::FormatWatermarkTimestamp(
          base::Time::FromSecondsSinceUnixEpoch(1700000000),
          timestamp_timezone);
    }
    if (auto* data_protection_overlay_view =
            BrowserView::GetBrowserViewForBrowser(browser())
                ->GetContentsContainerViews()[0]
                ->data_protection_overlay_view()) {
      data_protection_overlay_view->SetWatermarkText(
          message, kTestFillColor, kTestOutlineColor, kTestFontSize);
      return true;
    }
    return false;
  }

  void ShowUi(const std::string& name) override {
    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    // Use the test suffix to create a unique name for the golden image.
    const std::string name =
        std::string(test_info->name()) + "_" + GetParam().test_suffix;

    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest, WatermarkShownAfterNavigation) {
  NavigateToTestPage();
  ASSERT_TRUE(SetWatermark(GetParam().watermark_text));
  ShowAndVerifyUi();
}

#define MAYBE_WatermarkClearedAfterNavigation WatermarkClearedAfterNavigation
IN_PROC_BROWSER_TEST_P(WatermarkBrowserTest,
                       MAYBE_WatermarkClearedAfterNavigation) {
  ASSERT_TRUE(SetWatermark(GetParam().watermark_text));

  // Navigating away from a watermarked page should clear the watermark if no
  // other verdict/policy is present to show a watermark.
  NavigateToTestPage();
  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WatermarkBrowserTest,
    testing::Values(
        WatermarkTextParams{"Multilingual", kMultilingualWatermarkMessage},
        WatermarkTextParams{"LongLines", kLongLinesWatermarkMessage},
        WatermarkTextParams{"TimestampUtc", kTimestampUtcWatermarkMessage,
                            "UTC"},
        WatermarkTextParams{"TimestampTokyo", kTimestampTokyoWatermarkMessage,
                            "Asia/Tokyo"},
        WatermarkTextParams{"TimestampToronto",
                            kTimestampTorontoWatermarkMessage,
                            "America/Toronto"},
        WatermarkTextParams{"TimestampKolkata",
                            kTimestampKolkataWatermarkMessage, "Asia/Kolkata"},
        WatermarkTextParams{"TimestampInvalidFallback",
                            kTimestampUtcWatermarkMessage,
                            "Invalid/Timezone"}));

// Test fixture for the default chrome://watermark page.
class WatermarkTestPageBrowserTest : public UiBrowserTest {
 public:
  WatermarkTestPageBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkTestPage);
  }

  void ShowUi(const std::string& name) override {
    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         test_info->name()) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

namespace {


}  // namespace


// Test fixture for the dynamic chrome://watermark page tests. This is
// parameterized to test various style combinations.
class WatermarkTestPageDynamicBrowserTest
    : public UiBrowserTest,
      public testing::WithParamInterface<WatermarkParams> {
 public:
  WatermarkTestPageDynamicBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkTestPage);
  }

  void ShowUi(const std::string& name) override {
    base::RunLoop().RunUntilIdle();
  }

  bool VerifyUi() override {
    const auto* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    const std::string name =
        std::string(test_info->name()) + "_" + GetParam().test_suffix;

    return VerifyPixelUi(BrowserView::GetBrowserViewForBrowser(browser())
                             ->contents_container(),
                         test_info->test_suite_name(),
                         name) != ui::test::ActionResult::kFailed;
  }

  void WaitForUserDismissal() override {}

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WatermarkTestPageBrowserTest, InvokeUi_default) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIWatermarkURL)));
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_P(WatermarkTestPageDynamicBrowserTest, DynamicWatermark) {
  const auto& params = GetParam();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIWatermarkURL)));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* watermark_ui =
      web_contents->GetWebUI()->GetController()->GetAs<WatermarkUI>();
  ASSERT_TRUE(watermark_ui);
  auto* page_handler = watermark_ui->GetPageHandlerForTesting();
  ASSERT_TRUE(page_handler);

  auto settings = watermark::mojom::WatermarkSettings::New();
  settings->fill_opacity = params.fill_opacity;
  settings->outline_opacity = params.outline_opacity;
  settings->font_size = params.font_size;
  settings->watermark_text = params.watermark_text;
  page_handler->SetWatermarkSettings(std::move(settings));

  ShowAndVerifyUi();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    WatermarkTestPageDynamicBrowserTest,
    testing::Values(
        WatermarkParams{"HighOpacity", /*fill_opacity=*/80,
                        /*outline_opacity=*/90, /*font_size=*/24,
                        /*watermark_text=*/"Watermark"},
        WatermarkParams{"LargeFont", /*fill_opacity=*/4,
                        /*outline_opacity=*/6, /*font_size=*/72,
                        /*watermark_text=*/"Watermark"},
        WatermarkParams{"ZeroOpacity", /*fill_opacity=*/0,
                        /*outline_opacity=*/0, /*font_size=*/24,
                        /*watermark_text=*/"Watermark"},
        WatermarkParams{"Multilingual", /*fill_opacity=*/80,
                        /*outline_opacity=*/90, /*font_size=*/24,
                        /*watermark_text=*/kMultilingualWatermarkMessage},
        // Tests line wrapping behavior.
        WatermarkParams{"LongLines", /*fill_opacity=*/80,
                        /*outline_opacity=*/90, /*font_size=*/24,
                        /*watermark_text=*/kLongLinesWatermarkMessage}));

class WatermarkSettingsBrowserTest : public InProcessBrowserTest,
                                     public testing::WithParamInterface<bool> {
 public:
  WatermarkSettingsBrowserTest() {
    if (IsCustomizationEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(kEnableWatermarkCustomization);
    } else {
      scoped_feature_list_.InitAndDisableFeature(kEnableWatermarkCustomization);
    }
  }

  bool IsCustomizationEnabled() const { return GetParam(); }

  SkAlpha PercentageToSkAlpha(int percent_value) {
    return std::clamp(percent_value, 0, 100) * 255 / 100;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(WatermarkSettingsBrowserTest, GetStyleSettings) {
  PrefService* prefs = GetProfile()->GetPrefs();

  // Test with default pref values.
  SkColor expected_fill_color = GetDefaultFillColor();
  SkColor expected_outline_color = GetDefaultOutlineColor();
  int expected_font_size = GetDefaultFontSize();

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);
  EXPECT_EQ(GetFontSize(prefs), expected_font_size);

  // Test with custom pref values.
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleFillOpacityPref, 30);
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleOutlineOpacityPref,
                    40);
  prefs->SetInteger(enterprise_connectors::kWatermarkStyleFontSizePref, 50);

  if (IsCustomizationEnabled()) {
    expected_fill_color =
        SkColorSetA(SkColorSetRGB(0x00, 0x00, 0x00), PercentageToSkAlpha(30));
    expected_outline_color =
        SkColorSetA(SkColorSetRGB(0xff, 0xff, 0xff), PercentageToSkAlpha(40));
    expected_font_size = 50;
  }

  EXPECT_EQ(GetFillColor(prefs), expected_fill_color);
  EXPECT_EQ(GetOutlineColor(prefs), expected_outline_color);
  EXPECT_EQ(GetFontSize(prefs), expected_font_size);
}

INSTANTIATE_TEST_SUITE_P(All, WatermarkSettingsBrowserTest, testing::Bool());
class WatermarkSettingsCommandLineBrowserTest : public InProcessBrowserTest {
 public:
  SkAlpha PercentageToSkAlpha(int percent_value) {
    return std::clamp(percent_value, 0, 100) * 255 / 100;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII("watermark-fill-opacity", "50");
    command_line->AppendSwitchASCII("watermark-outline-opacity", "60");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      kEnableWatermarkCustomization};
};

IN_PROC_BROWSER_TEST_F(WatermarkSettingsCommandLineBrowserTest, GetColors) {
  PrefService* prefs = GetProfile()->GetPrefs();
  EXPECT_EQ(GetFillColor(prefs), SkColorSetA(SkColorSetRGB(0x00, 0x00, 0x00),
                                             PercentageToSkAlpha(50)));
  EXPECT_EQ(GetOutlineColor(prefs), SkColorSetA(SkColorSetRGB(0xff, 0xff, 0xff),
                                                PercentageToSkAlpha(60)));
}

}  // namespace enterprise_watermark
