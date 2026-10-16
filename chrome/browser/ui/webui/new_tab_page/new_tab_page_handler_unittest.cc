// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_handler.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/hash/hash.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/token.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_factory.h"
#include "chrome/browser/new_tab_page/microsoft_auth/microsoft_auth_service_observer.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/test/scoped_iph_feature_list.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/embedder/default_model/device_switcher_model.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/testing/mock_segmentation_platform_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider_source.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/gfx/color_palette.h"
#include "url/gurl.h"

#include "chrome/browser/themes/theme_helper.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/side_panel_controller_views.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/util/webui_util_desktop.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::ElementsAre;
using testing::SaveArg;

class MockPage : public new_tab_page::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<new_tab_page::mojom::Page> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetTheme, (new_tab_page::mojom::ThemePtr));
  MOCK_METHOD(void,
              SetDisabledModules,
              (bool, const std::vector<std::string>&));
  MOCK_METHOD(void, SetModulesLoadable, ());
  MOCK_METHOD(void, SetModulesFreVisibility, (bool));
  MOCK_METHOD(void, SetCustomizeChromeSidePanelVisibility, (bool));
  MOCK_METHOD(void, ShowWebstoreToast, ());
  MOCK_METHOD(void, SetWallpaperSearchButtonVisibility, (bool));
  MOCK_METHOD(void, FooterVisibilityUpdated, (bool));
  MOCK_METHOD(void,
              ConnectToParentDocument,
              (mojo::PendingRemote<
                  new_tab_page::mojom::MicrosoftAuthUntrustedDocument>));

  mojo::Receiver<new_tab_page::mojom::Page> receiver_{this};
};

class MockColorProviderSource : public ui::ColorProviderSource {
 public:
  MockColorProviderSource() = default;
  MockColorProviderSource(const MockColorProviderSource&) = delete;
  MockColorProviderSource& operator=(const MockColorProviderSource&) = delete;
  ~MockColorProviderSource() override = default;

  const ui::ColorProvider* GetColorProvider() const override {
    return &color_provider_;
  }

  ui::RendererColorMap GetRendererColorMap(
      ui::ColorProviderKey::ColorMode color_mode,
      ui::ColorProviderKey::ForcedColors forced_colors) const override {
    auto key = GetColorProviderKey();
    key.color_mode = color_mode;
    key.forced_colors = forced_colors;
    ui::ColorProvider* color_provider =
        ui::ColorProviderManager::Get().GetColorProviderFor(key);
    CHECK(color_provider);
    return ui::CreateRendererColorMap(*color_provider);
  }

  void SetColor(ui::ColorId id, SkColor color) {
    color_provider_.SetColorForTesting(id, color);
  }

 protected:
  ui::ColorProviderKey GetColorProviderKey() const override {
    return ui::ColorProviderKey();
  }

 private:
  ui::ColorProvider color_provider_;
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(void, RefreshBackgroundIfNeeded, ());
  MOCK_METHOD(void, VerifyCustomBackgroundImageURL, ());
  MOCK_METHOD(std::optional<CustomBackground>, GetCustomBackground, ());
  MOCK_METHOD(void, AddObserver, (NtpCustomBackgroundServiceObserver*));
};

class MockMicrosoftAuthService : public MicrosoftAuthService {
 public:
  MOCK_METHOD0(GetAuthState, MicrosoftAuthService::AuthState());
  MOCK_METHOD(void, AddObserver, (MicrosoftAuthServiceObserver*), (override));
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      MicrosoftAuthServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockMicrosoftAuthService>>();
      }));
  profile_builder.AddTestingFactory(
      OptimizationGuideKeyedServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<
            testing::NiceMock<MockOptimizationGuideKeyedService>>();
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
      profile.get(),
      base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
  return profile;
}

class MockThemeProvider : public ui::ThemeProvider {
 public:
  MOCK_CONST_METHOD1(GetImageSkiaNamed, gfx::ImageSkia*(int));
  MOCK_CONST_METHOD1(GetColor, SkColor(int));
  MOCK_CONST_METHOD1(GetTint, color_utils::HSL(int));
  MOCK_CONST_METHOD1(GetDisplayProperty, int(int));
  MOCK_CONST_METHOD0(ShouldUseNativeFrame, bool());
  MOCK_CONST_METHOD1(HasCustomImage, bool(int));
  MOCK_CONST_METHOD2(
      GetRawData,
      scoped_refptr<base::RefCountedMemory>(int, ui::ResourceScaleFactor));
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) {}
  MOCK_CONST_METHOD0(GetIsBaseline, bool());
  MOCK_CONST_METHOD0(GetThemeID, std::string());
  MOCK_CONST_METHOD0(GetUserColor, std::optional<SkColor>());
  MOCK_CONST_METHOD0(UsingAutogeneratedTheme, bool());
  MOCK_CONST_METHOD0(UsingDefaultTheme, bool());
  MOCK_CONST_METHOD0(UsingExtensionTheme, bool());
  MOCK_CONST_METHOD0(GetIsGrayscale, bool());
  MOCK_METHOD(void, AddObserver, (ThemeServiceObserver*));

 private:
  ThemeHelper theme_helper_;
};

class MockCustomizeChromeTabHelper
    : public customize_chrome::SidePanelController {
 public:
  ~MockCustomizeChromeTabHelper() override = default;

  MOCK_METHOD(bool, IsCustomizeChromeEntryAvailable, (), (const, override));
  MOCK_METHOD(bool, IsCustomizeChromeEntryShowing, (), (const, override));
  MOCK_METHOD(void,
              SetEntryChangedCallback,
              (StateChangedCallBack),
              (override));
  MOCK_METHOD(void,
              OpenSidePanel,
              (SidePanelOpenTrigger, std::optional<CustomizeChromeSection>),
              (override));
  MOCK_METHOD(void, CloseSidePanel, (), (override));
};

int GetDictPrefKeyCount(Profile* profile,
                        const std::string& pref_name,
                        const std::string& key) {
  const base::DictValue& counts_dict = profile->GetPrefs()->GetDict(pref_name);
  std::optional<int> count = counts_dict.FindInt(key);
  return count.has_value() ? count.value() : 0;
}

}  // namespace

class NewTabPageHandlerTest : public testing::Test {
 public:
  NewTabPageHandlerTest()
      : profile_(
            MakeTestingProfile(test_url_loader_factory_.GetSafeWeakWrapper())),
        mock_ntp_custom_background_service_(profile_.get()),
        web_contents_(factory_.CreateWebContents(profile_.get())),
        mock_customize_chrome_tab_helper_(
            std::make_unique<MockCustomizeChromeTabHelper>()) {
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), base::BindRepeating(&BuildMockHatsService)));
    EXPECT_CALL(*mock_hats_service(), CanShowAnySurvey(_))
        .WillRepeatedly(testing::Return(true));
  }

  ~NewTabPageHandlerTest() override = default;

  void SetUp() override {
    EXPECT_CALL(mock_theme_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&theme_service_observer_));
    EXPECT_CALL(mock_ntp_custom_background_service_, AddObserver)
        .Times(1)
        .WillOnce(
            testing::SaveArg<0>(&ntp_custom_background_service_observer_));
    if (!base::FeatureList::IsEnabled(
            ntp_features::kNtpBackgroundImageErrorDetection)) {
      EXPECT_CALL(mock_page_, SetTheme).Times(testing::AtLeast(1));
      EXPECT_CALL(mock_ntp_custom_background_service_,
                  RefreshBackgroundIfNeeded)
          .Times(testing::AtLeast(1));
    } else {
      EXPECT_CALL(mock_ntp_custom_background_service_,
                  VerifyCustomBackgroundImageURL)
          .Times(1);
    }
    web_contents_->SetColorProviderSource(&mock_color_provider_source_);

    webui::SetThemeProviderForTestingDeprecated(&mock_theme_provider_);
    EXPECT_FALSE(
        mock_customize_chrome_tab_helper_->IsCustomizeChromeEntryShowing());
    handler_ = std::make_unique<NewTabPageHandler>(
        mojo::PendingReceiver<new_tab_page::mojom::PageHandler>(),
        mock_page_.BindAndGetRemote(), profile_.get(),
        &mock_ntp_custom_background_service_, &mock_theme_service_,
        &test_sync_service_, &mock_segmentation_platform_service_,
        web_contents_, base::Time::Now(), base::TimeTicks::Now(),
        &module_id_details);
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), theme_service_observer_);
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
    testing::Mock::VerifyAndClearExpectations(
        &mock_ntp_custom_background_service_);
  }

 protected:
  testing::NiceMock<MockPage> mock_page_;
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNtpCustomBackgroundService>
      mock_ntp_custom_background_service_;
  syncer::TestSyncService test_sync_service_;
  segmentation_platform::MockSegmentationPlatformService
      mock_segmentation_platform_service_;
  MockColorProviderSource mock_color_provider_source_;
  content::TestWebContentsFactory factory_;
  raw_ptr<content::WebContents> web_contents_;  // Weak. Owned by factory_.
  base::HistogramTester histogram_tester_;

  MockHatsService* mock_hats_service() { return mock_hats_service_; }
  testing::NiceMock<MockThemeService> mock_theme_service_;
  testing::NiceMock<MockThemeProvider> mock_theme_provider_;
  std::unique_ptr<MockCustomizeChromeTabHelper>
      mock_customize_chrome_tab_helper_;

  std::unique_ptr<NewTabPageHandler> handler_;
  raw_ptr<ThemeServiceObserver> theme_service_observer_;
  raw_ptr<NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observer_;

 private:
  const std::vector<ntp::ModuleIdDetail> module_id_details = {
      {ntp_modules::kMicrosoftFilesModuleId,
       IDS_NTP_MODULES_MICROSOFT_FILES_NAME}};
  raw_ptr<MockHatsService> mock_hats_service_;
};

class NewTabPageHandlerThemeTest : public NewTabPageHandlerTest,
                                   public ::testing::WithParamInterface<bool> {
 public:
  NewTabPageHandlerThemeTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    if (BackgroundImageErrorDetection()) {
      enabled_features.push_back(
          ntp_features::kNtpBackgroundImageErrorDetection);
    } else {
      disabled_features.push_back(
          ntp_features::kNtpBackgroundImageErrorDetection);
    }

    feature_list_.InitWithFeatures(std::move(enabled_features),
                                   std::move(disabled_features));
  }

  bool BackgroundImageErrorDetection() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(NewTabPageHandlerThemeTest, SetGm2Theme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::optional<CustomBackground>()));
  mock_color_provider_source_.SetColor(kColorNewTabPageBackground,
                                       SkColorSetRGB(0, 0, 1));
  mock_color_provider_source_.SetColor(kColorNewTabPageText,
                                       SkColorSetRGB(0, 0, 2));
  mock_color_provider_source_.SetColor(kColorNewTabPageTextUnthemed,
                                       SkColorSetRGB(0, 0, 3));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, UsingAutogeneratedTheme())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_service_, GetUserColor())
      .WillByDefault(testing::Return(std::nullopt));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("bar"));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING))
      .WillByDefault(testing::Return(ThemeProperties::REPEAT_X));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT))
      .WillByDefault(testing::Return(ThemeProperties::ALIGN_TOP));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_ATTRIBUTION))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackground, SkColorSetRGB(0, 0, 6));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundThemed, SkColorSetRGB(0, 0, 7));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundUnthemed,
      SkColorSetRGB(0, 0, 8));

  theme_service_observer_->OnThemeChanged();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->is_dark);
  EXPECT_FALSE(theme->is_gm3);
  EXPECT_FALSE(theme->daily_refresh_enabled);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?bar",
            theme->background_image->url);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?bar",
            theme->background_image->url_2x);
  EXPECT_EQ("chrome://theme/IDR_THEME_NTP_ATTRIBUTION?bar",
            theme->background_image->attribution_url);
  EXPECT_EQ("initial", theme->background_image->size);
  EXPECT_EQ("repeat", theme->background_image->repeat_x);
  EXPECT_EQ("no-repeat", theme->background_image->repeat_y);
  EXPECT_EQ("center", theme->background_image->position_x);
  EXPECT_EQ("top", theme->background_image->position_y);
  EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->text_color);
  EXPECT_FALSE(theme->background_image_attribution_1.has_value());
  EXPECT_FALSE(theme->background_image_attribution_2.has_value());
  EXPECT_FALSE(theme->background_image_attribution_url.has_value());
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
  ASSERT_TRUE(theme->most_visited);
  EXPECT_EQ(SkColorSetRGB(0, 0, 6), theme->most_visited->background_color);
  EXPECT_TRUE(theme->most_visited->use_white_tile_icon);
  EXPECT_EQ(false, theme->most_visited->is_dark);
}

TEST_P(NewTabPageHandlerThemeTest, SetGm3Theme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::optional<CustomBackground>()));
  mock_color_provider_source_.SetColor(kColorNewTabPageBackground,
                                       SkColorSetRGB(0, 0, 1));
  mock_color_provider_source_.SetColor(kColorNewTabPageText,
                                       SkColorSetRGB(0, 0, 2));
  mock_color_provider_source_.SetColor(kColorNewTabPageTextUnthemed,
                                       SkColorSetRGB(0, 0, 3));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, UsingAutogeneratedTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, GetUserColor())
      .WillByDefault(testing::Return(SK_ColorWHITE));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("bar"));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_TILING))
      .WillByDefault(testing::Return(ThemeProperties::REPEAT_X));
  ON_CALL(mock_theme_provider_,
          GetDisplayProperty(ThemeProperties::NTP_BACKGROUND_ALIGNMENT))
      .WillByDefault(testing::Return(ThemeProperties::ALIGN_TOP));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_ATTRIBUTION))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackground, SkColorSetRGB(0, 0, 6));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundThemed, SkColorSetRGB(0, 0, 7));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundUnthemed,
      SkColorSetRGB(0, 0, 8));

  theme_service_observer_->OnThemeChanged();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0, 0, 1), theme->background_color);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->is_dark);
  EXPECT_TRUE(theme->is_gm3);
  EXPECT_FALSE(theme->daily_refresh_enabled);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND?bar",
            theme->background_image->url);
  EXPECT_EQ("chrome-untrusted://theme/IDR_THEME_NTP_BACKGROUND@2x?bar",
            theme->background_image->url_2x);
  EXPECT_EQ("chrome://theme/IDR_THEME_NTP_ATTRIBUTION?bar",
            theme->background_image->attribution_url);
  EXPECT_EQ("initial", theme->background_image->size);
  EXPECT_EQ("repeat", theme->background_image->repeat_x);
  EXPECT_EQ("no-repeat", theme->background_image->repeat_y);
  EXPECT_EQ("center", theme->background_image->position_x);
  EXPECT_EQ("top", theme->background_image->position_y);
  EXPECT_EQ(SkColorSetRGB(0, 0, 3), theme->text_color);
  EXPECT_FALSE(theme->background_image_attribution_1.has_value());
  EXPECT_FALSE(theme->background_image_attribution_2.has_value());
  EXPECT_FALSE(theme->background_image_attribution_url.has_value());
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
  ASSERT_TRUE(theme->most_visited);
  EXPECT_EQ(SkColorSetRGB(0, 0, 6), theme->most_visited->background_color);
  EXPECT_TRUE(theme->most_visited->use_white_tile_icon);
  EXPECT_EQ(false, theme->most_visited->is_dark);
}

TEST_P(NewTabPageHandlerThemeTest, SetCustomBackground) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.custom_background_attribution_line_1 = "foo line";
  custom_background.custom_background_attribution_line_2 = "bar line";
  custom_background.custom_background_attribution_action_url =
      GURL("https://foo.com/action");
  custom_background.collection_id = "baz collection";
  custom_background.daily_refresh_enabled = false;
  custom_background.is_uploaded_image = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  mock_color_provider_source_.SetColor(kColorNewTabPageBackground,
                                       SkColorSetRGB(0, 0, 1));
  mock_color_provider_source_.SetColor(kColorNewTabPageTextUnthemed,
                                       SkColorSetRGB(0, 0, 2));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackground, SkColorSetRGB(0, 0, 4));
  mock_color_provider_source_.SetColor(
      kColorNewTabPageMostVisitedTileBackgroundUnthemed,
      SkColorSetRGB(0, 0, 5));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_EQ(SkColorSetRGB(0, 0, 4), theme->most_visited->background_color);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->background_image_attribution_1.has_value());
  EXPECT_FALSE(theme->background_image_attribution_2.has_value());
  EXPECT_FALSE(theme->background_image_attribution_url.has_value());
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
}

TEST_P(NewTabPageHandlerThemeTest, SetDailyRefresh) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.daily_refresh_enabled = true;
  custom_background.is_uploaded_image = false;
  custom_background.collection_id = "baz collection";
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
}

TEST_P(NewTabPageHandlerThemeTest, SetUploadedImage) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.is_uploaded_image = true;
  custom_background.daily_refresh_enabled = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ("https://foo.com/img.png", theme->background_image->url);
  EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::kUploadedImage,
            theme->background_image->image_source);
}

TEST_P(NewTabPageHandlerThemeTest, SetWallpaperSearchImage) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.is_uploaded_image = true;
  custom_background.local_background_id = base::Token::CreateRandom();
  custom_background.is_inspiration_image = false;
  custom_background.daily_refresh_enabled = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::kWallpaperSearch,
            theme->background_image->image_source);
}

TEST_P(NewTabPageHandlerThemeTest, SetWallpaperSearchInspirationImage) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.is_uploaded_image = true;
  custom_background.local_background_id = base::Token::CreateRandom();
  custom_background.is_inspiration_image = true;
  custom_background.daily_refresh_enabled = false;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();

  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);
  EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::
                kWallpaperSearchInspiration,
            theme->background_image->image_source);
}

TEST_P(NewTabPageHandlerThemeTest, SetThirdPartyTheme) {
  new_tab_page::mojom::ThemePtr theme;
  EXPECT_CALL(mock_page_, SetTheme)
      .Times(1)
      .WillOnce([&theme](new_tab_page::mojom::ThemePtr arg) {
        theme = std::move(arg);
      });
  CustomBackground custom_background;
  custom_background.custom_background_url = GURL("https://foo.com/img.png");
  custom_background.collection_id = "baz collection";
  custom_background.daily_refresh_enabled = false;
  custom_background.is_uploaded_image = false;

  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(testing::Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_provider_, HasCustomImage(IDR_THEME_NTP_BACKGROUND))
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_service_, UsingDefaultTheme())
      .WillByDefault(testing::Return(false));
  ON_CALL(mock_theme_service_, UsingExtensionTheme())
      .WillByDefault(testing::Return(true));
  ON_CALL(mock_theme_service_, GetThemeID())
      .WillByDefault(testing::Return("foo"));

  ntp_custom_background_service_observer_->OnCustomBackgroundImageUpdated();
  mock_page_.FlushForTesting();
  ASSERT_TRUE(theme);
  ASSERT_TRUE(theme->background_image);

  EXPECT_FALSE(theme->is_custom_background);
  EXPECT_FALSE(theme->is_gm3);
  EXPECT_FALSE(theme->background_image_collection_id.has_value());
  EXPECT_EQ(new_tab_page::mojom::NtpBackgroundImageSource::kThirdPartyTheme,
            theme->background_image->image_source);
}

INSTANTIATE_TEST_SUITE_P(All, NewTabPageHandlerThemeTest, ::testing::Bool());

TEST_F(NewTabPageHandlerTest, Histograms) {
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 0);
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 0);

  handler_->OnDismissModule("shopping_tasks");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleDismissedHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleDismissedHistogram) +
          ".shopping_tasks",
      1);

  handler_->OnRestoreModule("kaleidoscope");
  histogram_tester_.ExpectTotalCount(
      NewTabPageHandler::kModuleRestoredHistogram, 1);
  histogram_tester_.ExpectTotalCount(
      std::string(NewTabPageHandler::kModuleRestoredHistogram) +
          ".kaleidoscope",
      1);
}

class NewTabPageHandlerMicrosoftAuthStateTest
    : public NewTabPageHandlerTest,
      public ::testing::WithParamInterface<MicrosoftAuthService::AuthState> {
 public:
  NewTabPageHandlerMicrosoftAuthStateTest() {
    profile_->GetTestingPrefService()->SetManagedPref(
        prefs::kNtpSharepointModuleVisible, base::Value(true));
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_features::kNtpMicrosoftAuthenticationModule,
                              ntp_features::kNtpSharepointModule,
                              ntp_features::kNtpOutlookCalendarModule},
        /*disabled_features=*/{});

    mock_microsoft_auth_service_ = static_cast<MockMicrosoftAuthService*>(
        MicrosoftAuthServiceFactory::GetForProfile(profile_.get()));
    EXPECT_CALL(*mock_microsoft_auth_service_, AddObserver)
        .Times(1)
        .WillOnce(testing::SaveArg<0>(&microsoft_auth_service_observer_));
  }

  MockMicrosoftAuthService& mock_microsoft_auth_service() {
    return *mock_microsoft_auth_service_;
  }

  MicrosoftAuthServiceObserver& microsoft_auth_service_observer() {
    return *microsoft_auth_service_observer_;
  }

  void SetAuthState(MicrosoftAuthService::AuthState state) {
    ON_CALL(mock_microsoft_auth_service(), GetAuthState())
        .WillByDefault(testing::Return(state));
  }

  MicrosoftAuthService::AuthState AuthState() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockMicrosoftAuthService> mock_microsoft_auth_service_;
  raw_ptr<MicrosoftAuthServiceObserver> microsoft_auth_service_observer_;
};

TEST_P(NewTabPageHandlerMicrosoftAuthStateTest, OnAuthStateUpdated) {
  SetAuthState(AuthState());

  microsoft_auth_service_observer().OnAuthStateUpdated();
  mock_page_.FlushForTesting();

  base::ListValue auth_dependent_modules;
  for (const char* id : ntp_modules::kMicrosoftAuthDependentModuleIds) {
    auth_dependent_modules.Append(id);
  }
  const std::string auth_id = ntp_modules::kMicrosoftAuthenticationModuleId;
  base::ListValue expected_disabled_modules;
  switch (AuthState()) {
    case MicrosoftAuthService::AuthState::kNone:
      break;
    case MicrosoftAuthService::AuthState::kError:
      expected_disabled_modules = std::move(auth_dependent_modules);
      break;
    case MicrosoftAuthService::AuthState::kSuccess:
      expected_disabled_modules.Append(auth_id);
      break;
  }

  EXPECT_EQ(profile_->GetPrefs()->GetList(prefs::kNtpHiddenModules),
            expected_disabled_modules);
}

TEST_P(NewTabPageHandlerMicrosoftAuthStateTest,
       UpdateModulesLoadableTriggersPageCall) {
  SetAuthState(AuthState());

  handler_->UpdateModulesLoadable();

  if (AuthState() == MicrosoftAuthService::AuthState::kNone) {
    EXPECT_CALL(mock_page_, SetModulesLoadable()).Times(0);
  } else {
    EXPECT_CALL(mock_page_, SetModulesLoadable()).Times(1);
  }

  mock_page_.FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(
    All,
    NewTabPageHandlerMicrosoftAuthStateTest,
    ::testing::Values(MicrosoftAuthService::AuthState::kNone,
                      MicrosoftAuthService::AuthState::kError,
                      MicrosoftAuthService::AuthState::kSuccess));

TEST_F(NewTabPageHandlerTest, GetModulesIdNames) {
  std::vector<new_tab_page::mojom::ModuleIdNamePtr> modules_details;
  base::MockCallback<NewTabPageHandler::GetModulesIdNamesCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .Times(1)
      .WillOnce([&modules_details](
                    std::vector<new_tab_page::mojom::ModuleIdNamePtr> arg) {
        modules_details = std::move(arg);
      });
  handler_->GetModulesIdNames(callback.Get());
  EXPECT_EQ(modules_details.size(), 1u);
  EXPECT_EQ(modules_details.front()->id, ntp_modules::kMicrosoftFilesModuleId);
}

TEST_F(NewTabPageHandlerTest, GetModulesOrder) {
  std::vector<std::string> module_ids;
  base::MockCallback<NewTabPageHandler::GetModulesOrderCallback> callback;
  EXPECT_CALL(callback, Run(_)).Times(1).WillOnce(SaveArg<0>(&module_ids));
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {{ntp_features::kNtpModulesOrder,
        {{ntp_features::kNtpModulesOrderParam, "bar,baz,microsoft_files"}}},
       {ntp_features::kNtpModulesDragAndDrop, {}}},
      {});
  base::ListValue module_ids_value;
  module_ids_value.Append("foo");
  module_ids_value.Append("bar");
  profile_->GetPrefs()->SetList(prefs::kNtpModulesOrder,
                                std::move(module_ids_value));

  handler_->GetModulesOrder(callback.Get());
  EXPECT_THAT(module_ids, ElementsAre("foo", "bar", "baz", "microsoft_files",
                                      "microsoft_authentication",
                                      "outlook_calendar", "tab_resumption"));
}

class NewTabPageHandlerModuleRemovalTest : public NewTabPageHandlerTest {
 public:
  // Wrapper around GetModulesEligibleForRemoval, for deduping the callback.
  std::vector<std::string> WrapGetModulesEligibleForRemoval() {
    std::vector<std::string> result;
    base::MockCallback<NewTabPageHandler::GetModulesEligibleForRemovalCallback>
        callback;
    EXPECT_CALL(callback, Run(_))
        .Times(1)
        .WillOnce([&result](std::vector<std::string> arg) {
          result = std::move(arg);
        });

    handler_->GetModulesEligibleForRemoval(callback.Get());

    return result;
  }
};

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalFeatureDisabled) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {}, {
              {ntp_features::kNtpFeatureOptimizationModuleRemoval},
          });

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(0u, removed_modules.size());
}

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalIsManagedPreference) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpFeatureOptimizationModuleRemoval,
           {
               {"ModuleMinStalenessUpdateTimeInterval", "24h"},
               {"StaleModulesCountThreshold", "14"},
           }},
      },
      {});

  profile_->GetTestingPrefService()->SetManagedPref(prefs::kNtpModulesVisible,
                                                    base::Value(true));

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(0u, removed_modules.size());
}

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalAllModulesForceDisabled) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpFeatureOptimizationModuleRemoval,
           {
               {"ModuleMinStalenessUpdateTimeInterval", "24h"},
               {"StaleModulesCountThreshold", "14"},
           }},
      },
      {});

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  update->Set(ntp_modules::kAllModulesId, true);

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(0u, removed_modules.size());
}

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalModuleForceDisabled) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpFeatureOptimizationModuleRemoval,
           {
               {"ModuleMinStalenessUpdateTimeInterval", "24h"},
               {"StaleModulesCountThreshold", "14"},
           }},
      },
      {});

  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  update->Set(ntp_modules::kMicrosoftFilesModuleId, true);

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(0u, removed_modules.size());
}

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalModuleBelowStalenessThreshold) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpFeatureOptimizationModuleRemoval,
           {
               {"ModuleMinStalenessUpdateTimeInterval", "24h"},
               {"StaleModulesCountThreshold", "14"},
           }},
      },
      {});

  const int below_staleness_threshold = 1;
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              ntp_prefs::kNtpModuleStalenessCountDict);
  update->Set(ntp_modules::kMicrosoftFilesModuleId, below_staleness_threshold);

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(0u, removed_modules.size());
}

TEST_F(NewTabPageHandlerModuleRemovalTest,
       GetModulesEligibleForRemovalModuleAboveStalenessThreshold) {
  // Arrange.
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {ntp_features::kNtpFeatureOptimizationModuleRemoval,
           {
               {"ModuleMinStalenessUpdateTimeInterval", "24h"},
               {"StaleModulesCountThreshold", "14"},
           }},
      },
      {});

  const int above_staleness_threshold = 100;
  ScopedDictPrefUpdate update(profile_->GetPrefs(),
                              ntp_prefs::kNtpModuleStalenessCountDict);
  update->Set(ntp_modules::kMicrosoftFilesModuleId, above_staleness_threshold);

  // Act.
  std::vector<std::string> removed_modules = WrapGetModulesEligibleForRemoval();

  // Assert.
  EXPECT_EQ(1u, removed_modules.size());
  EXPECT_TRUE(std::ranges::contains(removed_modules,
                                    ntp_modules::kMicrosoftFilesModuleId));
}

TEST_F(NewTabPageHandlerTest, SetModulesDisabledTrueDisabledAndTrueUserAction) {
  // Arrange.
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& initial_disabled_modules_list = update.Get();
  initial_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);

  std::vector<std::string> set_disabled_modules_true = {
      ntp_modules::kMicrosoftFilesModuleId,
      ntp_modules::kTabGroupsModuleId,
  };

  base::ListValue expected_disabled_modules_list;
  expected_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);
  expected_disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  expected_disabled_modules_list.Append(ntp_modules::kTabGroupsModuleId);

  // Act.
  handler_->SetModulesDisabled(set_disabled_modules_true, /*disabled=*/true,
                               /*is_user_action=*/true);

  // Assert.
  EXPECT_EQ(expected_disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));

  const base::DictValue& removal_disabled_dict = profile_->GetPrefs()->GetDict(
      ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  EXPECT_TRUE(
      removal_disabled_dict.FindBool(ntp_modules::kMicrosoftFilesModuleId)
          .value_or(false));
  EXPECT_TRUE(removal_disabled_dict.FindBool(ntp_modules::kTabGroupsModuleId)
                  .value_or(false));

  const base::DictValue& interacted_count_dict =
      profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
  EXPECT_EQ(1,
            interacted_count_dict.FindInt(ntp_modules::kMicrosoftFilesModuleId)
                .value_or(0));
  EXPECT_EQ(1, interacted_count_dict.FindInt(ntp_modules::kTabGroupsModuleId)
                   .value_or(0));
}

TEST_F(NewTabPageHandlerTest,
       SetModulesDisabledFalseDisabledAndTrueUserAction) {
  // Arrange.
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& initial_disabled_modules_list = update.Get();
  initial_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kTabGroupsModuleId);

  std::vector<std::string> set_disabled_modules_false = {
      ntp_modules::kMicrosoftFilesModuleId,
      ntp_modules::kTabGroupsModuleId,
  };

  base::ListValue expected_disabled_modules_list;
  expected_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);

  // Act.
  handler_->SetModulesDisabled(set_disabled_modules_false, /*disabled=*/false,
                               /*is_user_action=*/true);

  // Assert.
  EXPECT_EQ(expected_disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));
  const base::DictValue& removal_disabled_dict = profile_->GetPrefs()->GetDict(
      ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  EXPECT_TRUE(
      removal_disabled_dict.FindBool(ntp_modules::kMicrosoftFilesModuleId)
          .value_or(false));
  EXPECT_TRUE(removal_disabled_dict.FindBool(ntp_modules::kTabGroupsModuleId)
                  .value_or(false));

  const base::DictValue& interacted_count_dict =
      profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
  EXPECT_EQ(1,
            interacted_count_dict.FindInt(ntp_modules::kMicrosoftFilesModuleId)
                .value_or(0));
  EXPECT_EQ(1, interacted_count_dict.FindInt(ntp_modules::kTabGroupsModuleId)
                   .value_or(0));
}

TEST_F(NewTabPageHandlerTest,
       SetModulesDisabledTrueDisabledAndFalseUserAction) {
  // Arrange.
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& initial_disabled_modules_list = update.Get();
  initial_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);

  std::vector<std::string> set_disabled_modules_true = {
      ntp_modules::kMicrosoftFilesModuleId,
      ntp_modules::kTabGroupsModuleId,
  };

  base::ListValue expected_disabled_modules_list;
  expected_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);
  expected_disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  expected_disabled_modules_list.Append(ntp_modules::kTabGroupsModuleId);

  // Act.
  handler_->SetModulesDisabled(set_disabled_modules_true, /*disabled=*/true,
                               /*is_user_action=*/false);

  // Assert.
  EXPECT_EQ(expected_disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));

  const base::DictValue& removal_disabled_dict = profile_->GetPrefs()->GetDict(
      ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  EXPECT_TRUE(
      removal_disabled_dict.FindBool(ntp_modules::kMicrosoftFilesModuleId)
          .value_or(false));
  EXPECT_TRUE(removal_disabled_dict.FindBool(ntp_modules::kTabGroupsModuleId)
                  .value_or(false));

  const base::DictValue& interacted_count_dict =
      profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
  EXPECT_EQ(0,
            interacted_count_dict.FindInt(ntp_modules::kMicrosoftFilesModuleId)
                .value_or(0));
  EXPECT_EQ(0, interacted_count_dict.FindInt(ntp_modules::kTabGroupsModuleId)
                   .value_or(0));
}

TEST_F(NewTabPageHandlerTest,
       SetModulesDisabledFalseDisabledAndFalseUserAction) {
  // Arrange.
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& initial_disabled_modules_list = update.Get();
  initial_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kTabGroupsModuleId);

  std::vector<std::string> set_disabled_modules_false = {
      ntp_modules::kMicrosoftFilesModuleId,
      ntp_modules::kTabGroupsModuleId,
  };

  base::ListValue expected_disabled_modules_list;
  expected_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);

  // Act.
  handler_->SetModulesDisabled(set_disabled_modules_false, /*disabled=*/false,
                               /*is_user_action=*/false);

  // Assert.
  EXPECT_EQ(expected_disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));

  const base::DictValue& removal_disabled_dict = profile_->GetPrefs()->GetDict(
      ntp_prefs::kNtpModulesAutoRemovalDisabledDict);
  EXPECT_TRUE(
      removal_disabled_dict.FindBool(ntp_modules::kMicrosoftFilesModuleId)
          .value_or(false));
  EXPECT_TRUE(removal_disabled_dict.FindBool(ntp_modules::kTabGroupsModuleId)
                  .value_or(false));

  const base::DictValue& interacted_count_dict =
      profile_->GetPrefs()->GetDict(prefs::kNtpModulesInteractedCountDict);
  EXPECT_EQ(0,
            interacted_count_dict.FindInt(ntp_modules::kMicrosoftFilesModuleId)
                .value_or(0));
  EXPECT_EQ(0, interacted_count_dict.FindInt(ntp_modules::kTabGroupsModuleId)
                   .value_or(0));
}

TEST_F(NewTabPageHandlerTest, SetModulesDisabledEmptyList) {
  // Arrange.
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::ListValue& initial_disabled_modules_list = update.Get();
  initial_disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kTabGroupsModuleId);
  initial_disabled_modules_list.Append(ntp_modules::kOutlookCalendarModuleId);

  std::vector<std::string> set_disabled_modules_empty = {};

  // Act.
  handler_->SetModulesDisabled(set_disabled_modules_empty, /*disabled=*/true,
                               /*is_user_action=*/true);

  // Assert.
  EXPECT_EQ(initial_disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));
}

TEST_F(NewTabPageHandlerTest, SurveyLaunchedEligibleModulesCriteria) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {features::kHappinessTrackingSurveysForDesktopNtpModules,
           {{ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam,
             "tab_groups,microsoft_files"}}},
      },
      {});

  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(1);
  const std::vector<std::string> module_ids = {
      ntp_modules::kTabGroupsModuleId,
      ntp_modules::kMostRelevantTabResumptionModuleId};
  handler_->OnModulesLoadedWithData(module_ids);

  for (const auto& module_id : module_ids) {
    EXPECT_EQ(
        1, GetDictPrefKeyCount(profile_.get(),
                               prefs::kNtpModulesLoadedCountDict, module_id));
  }
}

TEST_F(NewTabPageHandlerTest, SurveyLaunchSkippedEligibleModulesCriteria) {
  base::test::ScopedFeatureList features;
  features.InitWithFeaturesAndParameters(
      {
          {features::kHappinessTrackingSurveysForDesktopNtpModules,
           {{ntp_features::kNtpModulesEligibleForHappinessTrackingSurveyParam,
             ntp_modules::kMicrosoftFilesModuleId}}},
      },
      {});

  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(_, _, _, _, _, _, _, _, _, _))
      .Times(0);
  const std::vector<std::string> module_ids = {ntp_modules::kTabGroupsModuleId};
  handler_->OnModulesLoadedWithData(module_ids);

  for (const auto& module_id : module_ids) {
    EXPECT_EQ(
        1, GetDictPrefKeyCount(profile_.get(),
                               prefs::kNtpModulesLoadedCountDict, module_id));
  }
}

TEST_F(NewTabPageHandlerTest, SetModuleDisabled) {
  base::ListValue disabled_modules_list;
  EXPECT_EQ(disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));

  std::vector<std::string> module_ids = {ntp_modules::kMicrosoftFilesModuleId};
  handler_->SetModulesDisabled(module_ids, /*disabled=*/true,
                               /*is_user_action=*/true);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();

  disabled_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  EXPECT_EQ(disabled_modules_list,
            profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules));

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
                  .FindBool(ntp_modules::kMicrosoftFilesModuleId)
                  .value_or(false));
}

TEST_F(NewTabPageHandlerTest, SetModuleHiddenAndDisabled) {
  bool all;
  std::vector<std::string> disabled_module_ids;
  EXPECT_CALL(mock_page_, SetDisabledModules)
      .Times(2)
      .WillRepeatedly(
          [&all, &disabled_module_ids](
              bool all_arg, std::vector<std::string> module_ids_arg) {
            all = all_arg;
            disabled_module_ids = std::move(module_ids_arg);
          });
  mock_page_.FlushForTesting();

  base::ListValue hidden_modules_list;
  hidden_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  profile_->GetPrefs()->SetList(prefs::kNtpHiddenModules,
                                std::move(hidden_modules_list));
  mock_page_.FlushForTesting();
  EXPECT_FALSE(all);
  EXPECT_EQ(1u, disabled_module_ids.size());
  EXPECT_EQ(disabled_module_ids[0], ntp_modules::kMicrosoftFilesModuleId);

  std::vector<std::string> module_ids = {ntp_modules::kMicrosoftFilesModuleId};
  handler_->SetModulesDisabled(module_ids, /*disabled=*/true,
                               /*is_user_action=*/true);
  mock_page_.FlushForTesting();
  // Ensure |disabled_module_ids| still only has one entry for
  // `ntp_modules::kMicrosoftFilesModuleId`.
  EXPECT_FALSE(all);
  EXPECT_EQ(1u, disabled_module_ids.size());
  EXPECT_EQ(disabled_module_ids[0], ntp_modules::kMicrosoftFilesModuleId);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
                  .FindBool(ntp_modules::kMicrosoftFilesModuleId)
                  .value_or(false));
}

TEST_F(NewTabPageHandlerTest, SetModuleHiddenAndDisabledCardsManagedVisible) {
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kNtpModulesVisible,
                                                    base::Value(true));
  bool all;
  std::vector<std::string> disabled_module_ids;
  EXPECT_CALL(mock_page_, SetDisabledModules)
      .Times(3)
      .WillRepeatedly(
          [&all, &disabled_module_ids](
              bool all_arg, std::vector<std::string> module_ids_arg) {
            all = all_arg;
            disabled_module_ids = std::move(module_ids_arg);
          });
  mock_page_.FlushForTesting();

  // Managed card visibility should ignore disabling of cards.
  std::vector<std::string> module_ids = {ntp_modules::kMicrosoftFilesModuleId};
  handler_->SetModulesDisabled(module_ids, /*disabled=*/true,
                               /*is_user_action=*/true);
  mock_page_.FlushForTesting();
  EXPECT_FALSE(all);
  EXPECT_TRUE(disabled_module_ids.empty());

  // Managed card visibility that forces display of cards should respect
  // hidden cards.
  base::ListValue hidden_modules_list;
  hidden_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  profile_->GetPrefs()->SetList(prefs::kNtpHiddenModules,
                                std::move(hidden_modules_list));
  mock_page_.FlushForTesting();
  EXPECT_FALSE(all);
  EXPECT_EQ(1u, disabled_module_ids.size());
  EXPECT_EQ(disabled_module_ids[0], ntp_modules::kMicrosoftFilesModuleId);

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
                  .FindBool(ntp_modules::kMicrosoftFilesModuleId)
                  .value_or(false));
}

TEST_F(NewTabPageHandlerTest,
       SetModuleHiddenAndDisabledCardsManagedNotVisible) {
  profile_->GetTestingPrefService()->SetManagedPref(prefs::kNtpModulesVisible,
                                                    base::Value(false));
  bool all;
  std::vector<std::string> disabled_module_ids;
  EXPECT_CALL(mock_page_, SetDisabledModules)
      .Times(3)
      .WillRepeatedly(
          [&all, &disabled_module_ids](
              bool all_arg, std::vector<std::string> module_ids_arg) {
            all = all_arg;
            disabled_module_ids = std::move(module_ids_arg);
          });
  mock_page_.FlushForTesting();

  // Managed card visibility of cards should ignore hidden and disabled cards
  // and send a value of true for all cards being disabled.
  base::ListValue hidden_modules_list;
  hidden_modules_list.Append(ntp_modules::kMicrosoftFilesModuleId);
  profile_->GetPrefs()->SetList(prefs::kNtpHiddenModules,
                                std::move(hidden_modules_list));
  mock_page_.FlushForTesting();
  EXPECT_TRUE(all);
  EXPECT_TRUE(disabled_module_ids.empty());

  std::vector<std::string> module_ids = {ntp_modules::kMicrosoftFilesModuleId};
  handler_->SetModulesDisabled(module_ids, /*disabled=*/true,
                               /*is_user_action=*/true);
  mock_page_.FlushForTesting();
  EXPECT_TRUE(all);
  EXPECT_TRUE(disabled_module_ids.empty());

  EXPECT_TRUE(profile_->GetPrefs()
                  ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
                  .FindBool(ntp_modules::kMicrosoftFilesModuleId)
                  .value_or(false));
}

TEST_F(NewTabPageHandlerTest, SetModulesVisible) {
  handler_->SetModulesVisible(true);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible));

  const std::optional<bool> all_modules_auto_removal_disabled =
      profile_->GetPrefs()
          ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
          .FindBool(ntp_modules::kAllModulesId);
  EXPECT_TRUE(all_modules_auto_removal_disabled.value_or(false));
}

TEST_F(NewTabPageHandlerTest, SetModulesNotVisible) {
  handler_->SetModulesVisible(false);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();

  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible));

  const std::optional<bool> all_modules_auto_removal_disabled =
      profile_->GetPrefs()
          ->GetDict(ntp_prefs::kNtpModulesAutoRemovalDisabledDict)
          .FindBool(ntp_modules::kAllModulesId);
  EXPECT_TRUE(all_modules_auto_removal_disabled.value_or(false));
}

TEST_F(NewTabPageHandlerTest, ModulesVisiblePrefChangeTriggersPageCall) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, true);
  EXPECT_CALL(mock_page_, SetDisabledModules).Times(1);
  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, ShowWebstoreToast) {
  profile_->GetPrefs()->SetInteger(prefs::kSeedColorChangeCount, 1);

  EXPECT_CALL(mock_page_, ShowWebstoreToast).Times(1);
  mock_page_.FlushForTesting();
}

TEST_F(NewTabPageHandlerTest, DoNotShowWebstoreToastOnCountExceeded) {
  profile_->GetPrefs()->SetInteger(prefs::kSeedColorChangeCount, 4);

  EXPECT_CALL(mock_page_, ShowWebstoreToast).Times(0);
  mock_page_.FlushForTesting();
}

class NewTabPageHandlerHaTSTest : public NewTabPageHandlerTest {
 public:
  static constexpr char kSampleModuleId[] = "sample_module_id";
  static constexpr char kSampleTriggerId[] = "sample_trigger_id";
  static constexpr int kSampleDelayTimeMs = 15000;
  static constexpr int kSampleIgnoreCriteriaThreshold = 20;

  NewTabPageHandlerHaTSTest() {
    auto interaction_module_trigger_ids_dict = base::DictValue();
    const auto kInteractionNames =
        std::array<std::string, 4>{"disable", "dismiss", "ignore", "use"};
    for (const auto& interaction_name : kInteractionNames) {
      interaction_module_trigger_ids_dict.Set(
          interaction_name,
          base::DictValue().Set(kSampleModuleId, kSampleTriggerId));
    }

    base::test::ScopedFeatureList features;
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kHappinessTrackingSurveysForDesktopNtpModules,
             {{ntp_features::kNtpModulesInteractionBasedSurveyEligibleIdsParam,
               base::WriteJson(interaction_module_trigger_ids_dict).value()},
              {ntp_features::kNtpModuleIgnoredHaTSDelayTimeParam,
               base::NumberToString(kSampleDelayTimeMs)},
              {ntp_features::kNtpModuleIgnoredCriteriaThreshold,
               base::NumberToString(kSampleIgnoreCriteriaThreshold)}}},
        },
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(NewTabPageHandlerHaTSTest, ModuleInteractionTriggersHaTS) {
  const auto& kSampleModuleId = NewTabPageHandlerHaTSTest::kSampleModuleId;
  const size_t kInteractionNamesCount = 3;
  const auto kInteractionNames =
      std::array<std::string, kInteractionNamesCount>{"disable", "dismiss",
                                                      "use"};
  for (const auto& interaction : kInteractionNames) {
    int timeout_ms;
    std::optional<std::string> supplied_trigger_id;
    EXPECT_CALL(*mock_hats_service(),
                LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerNtpModules,
                                                  web_contents_.get(), _, _, _,
                                                  _, _, _, _, _))
        .Times(1)
        .WillOnce(DoAll(SaveArg<2>(&timeout_ms),
                        SaveArg<8>(&supplied_trigger_id),
                        testing::Return(HatsService::LaunchError::kNone)));

    if (interaction == "disable") {
      handler_->SetModulesDisabled({kSampleModuleId}, /*disabled=*/true,
                                   /*is_user_action=*/true);
    } else if (interaction == "dismiss") {
      handler_->OnDismissModule(kSampleModuleId);
    } else if (interaction == "use") {
      handler_->OnModuleUsed(kSampleModuleId);
    }

    const int kExpectedTimeoutMs = 0;
    EXPECT_EQ(kExpectedTimeoutMs, timeout_ms);
    EXPECT_EQ(NewTabPageHandlerHaTSTest::kSampleTriggerId,
              supplied_trigger_id.value());
  }

  EXPECT_EQ(
      static_cast<int>(kInteractionNamesCount),
      GetDictPrefKeyCount(profile_.get(), prefs::kNtpModulesInteractedCountDict,
                          kSampleModuleId));
}

TEST_F(NewTabPageHandlerHaTSTest, IgnoredModuleTriggersHaTS) {
  profile_->GetPrefs()->SetDict(
      prefs::kNtpModulesLoadedCountDict,
      base::DictValue().Set(
          NewTabPageHandlerHaTSTest::kSampleModuleId,
          NewTabPageHandlerHaTSTest::kSampleIgnoreCriteriaThreshold));
  profile_->GetPrefs()->SetDict(
      prefs::kNtpModulesInteractedCountDict,
      base::DictValue().Set(NewTabPageHandlerHaTSTest::kSampleModuleId, 0));

  int timeout_ms;
  std::optional<std::string> supplied_trigger_id;
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerNtpModules,
                                                web_contents_.get(), _, _, _, _,
                                                _, _, _, _))
      .Times(1)
      .WillOnce(DoAll(SaveArg<2>(&timeout_ms), SaveArg<8>(&supplied_trigger_id),
                      testing::Return(HatsService::LaunchError::kNone)));
  const std::vector<std::string> module_ids = {
      NewTabPageHandlerHaTSTest::kSampleModuleId};
  handler_->OnModulesLoadedWithData(module_ids);
  EXPECT_EQ(NewTabPageHandlerHaTSTest::kSampleDelayTimeMs, timeout_ms);
  EXPECT_EQ(NewTabPageHandlerHaTSTest::kSampleTriggerId,
            supplied_trigger_id.value());
}

TEST_F(NewTabPageHandlerHaTSTest, InteractedModuleDoesNotTriggerIgnoredHaTS) {
  profile_->GetPrefs()->SetDict(
      prefs::kNtpModulesLoadedCountDict,
      base::DictValue().Set(
          NewTabPageHandlerHaTSTest::kSampleModuleId,
          NewTabPageHandlerHaTSTest::kSampleIgnoreCriteriaThreshold - 1));
  profile_->GetPrefs()->SetDict(
      prefs::kNtpModulesInteractedCountDict,
      base::DictValue().Set(NewTabPageHandlerHaTSTest::kSampleModuleId, 1));

  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurveyForWebContents(kHatsSurveyTriggerNtpModules,
                                                web_contents_.get(), _, _, _, _,
                                                _, _, _, _))
      .Times(0);
  const std::vector<std::string> module_ids = {
      NewTabPageHandlerHaTSTest::kSampleModuleId};
  handler_->OnModulesLoadedWithData(module_ids);
  EXPECT_EQ(
      kSampleIgnoreCriteriaThreshold,
      GetDictPrefKeyCount(profile_.get(), prefs::kNtpModulesLoadedCountDict,
                          NewTabPageHandlerHaTSTest::kSampleModuleId));
}
