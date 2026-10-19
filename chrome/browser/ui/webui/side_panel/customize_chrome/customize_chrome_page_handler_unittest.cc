// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_move_support.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/background/ntp_custom_background_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/native_theme.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace {

using testing::DoAll;
using testing::Mock;
using testing::Return;
using testing::SaveArg;

class MockPage : public side_panel::mojom::CustomizeChromePage {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<side_panel::mojom::CustomizeChromePage>
  BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  void FlushForTesting() { receiver_.FlushForTesting(); }

  MOCK_METHOD(void, SetTheme, (side_panel::mojom::ThemePtr));
  MOCK_METHOD(void, ScrollToSection, (CustomizeChromeSection));
  MOCK_METHOD(void,
              AttachedTabStateUpdated,
              (side_panel::mojom::NewTabPageType));
  MOCK_METHOD(void,
              NtpManagedByNameUpdated,
              (const std::string&, const std::string&));
  MOCK_METHOD(
      void,
      SetFooterSettings,
      (bool visible,
       bool extension_policy_enabled));

  mojo::Receiver<side_panel::mojom::CustomizeChromePage> receiver_{this};
};

class MockNtpCustomBackgroundService : public NtpCustomBackgroundService {
 public:
  explicit MockNtpCustomBackgroundService(Profile* profile)
      : NtpCustomBackgroundService(profile) {}
  MOCK_METHOD(std::optional<CustomBackground>, GetCustomBackground, ());
  MOCK_METHOD(void, ResetCustomBackgroundInfo, ());
  MOCK_METHOD(void, AddObserver, (NtpCustomBackgroundServiceObserver*));
  MOCK_METHOD(bool, IsCustomBackgroundDisabledByPolicy, ());
};

class MockThemeService : public ThemeService {
 public:
  MockThemeService() : ThemeService(nullptr, theme_helper_) { set_ready(); }
  using ThemeService::NotifyThemeChanged;
  MOCK_METHOD(void, UseDefaultTheme, ());
  MOCK_CONST_METHOD0(UsingDefaultTheme, bool());
  MOCK_CONST_METHOD0(UsingSystemTheme, bool());
  MOCK_CONST_METHOD0(UsingExtensionTheme, bool());
  MOCK_CONST_METHOD0(GetThemeID, std::string());

 private:
  ThemeHelper theme_helper_;
};

std::unique_ptr<TestingProfile> MakeTestingProfile(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      ThemeServiceFactory::GetInstance(),
      base::BindRepeating([](content::BrowserContext* context)
                              -> std::unique_ptr<KeyedService> {
        return std::make_unique<testing::NiceMock<MockThemeService>>();
      }));
  profile_builder.SetSharedURLLoaderFactory(url_loader_factory);
  auto profile = profile_builder.Build();
  return profile;
}

}  // namespace

class CustomizeChromePageHandlerTest : public testing::Test {
 public:
  CustomizeChromePageHandlerTest()
      : profile_(
            MakeTestingProfile(test_url_loader_factory_.GetSafeWeakWrapper())),
        mock_ntp_custom_background_service_(profile_.get()),
        web_contents_(web_contents_factory_.CreateWebContents(profile_.get())),
        mock_theme_service_(static_cast<MockThemeService*>(
            ThemeServiceFactory::GetForProfile(profile_.get()))) {}

  void SetUp() override {
    EXPECT_CALL(mock_ntp_custom_background_service_, AddObserver)
        .Times(1)
        .WillOnce(SaveArg<0>(&ntp_custom_background_service_observer_));
    handler_ = std::make_unique<CustomizeChromePageHandler>(
        mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>(),
        mock_page_.BindAndGetRemote(), &mock_ntp_custom_background_service_,
        web_contents_, mock_open_url_callback_.Get());
    mock_page_.FlushForTesting();
    EXPECT_EQ(handler_.get(), ntp_custom_background_service_observer_);

    scoped_feature_list_.Reset();
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    test_url_loader_factory_.ClearResponses();
  }

  CustomizeChromePageHandler& handler() { return *handler_; }
  NtpCustomBackgroundServiceObserver& ntp_custom_background_service_observer() {
    return *ntp_custom_background_service_observer_;
  }
  MockThemeService& mock_theme_service() { return *mock_theme_service_; }

 protected:
  // NOTE: The initialization order of these members matters.
  content::BrowserTaskEnvironment task_environment_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockNtpCustomBackgroundService>
      mock_ntp_custom_background_service_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockPage> mock_page_;
  raw_ptr<MockThemeService> mock_theme_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::MockRepeatingCallback<void(const GURL& gurl)> mock_open_url_callback_;
  std::unique_ptr<CustomizeChromePageHandler> handler_;
  raw_ptr<NtpCustomBackgroundServiceObserver>
      ntp_custom_background_service_observer_;
};

enum class ThemeUpdateSource {
  kMojo,
  kThemeService,
  kNativeTheme,
  kCustomBackgroundService,
};

class CustomizeChromePageHandlerSetThemeTest
    : public CustomizeChromePageHandlerTest,
      public ::testing::WithParamInterface<ThemeUpdateSource> {
 public:
  side_panel::mojom::ThemePtr UpdateTheme() {
    // Flush any existing updates so the flush below only sees what results from
    // the update below.
    mock_page_.FlushForTesting();

    side_panel::mojom::ThemePtr theme;
    EXPECT_CALL(mock_page_, SetTheme).Times(1).WillOnce(MoveArg(&theme));

    // Update.
    switch (GetParam()) {
      case ThemeUpdateSource::kMojo:
        handler().UpdateTheme();
        break;
      case ThemeUpdateSource::kThemeService:
        mock_theme_service().NotifyThemeChanged();
        break;
      case ThemeUpdateSource::kNativeTheme:
        ui::NativeTheme::GetInstanceForNativeUi()->NotifyOnNativeThemeUpdated();
        break;
      case ThemeUpdateSource::kCustomBackgroundService:
        ntp_custom_background_service_observer()
            .OnCustomBackgroundImageUpdated();
        break;
    }
    mock_page_.FlushForTesting();

    // Should have seen exactly one attempt to set the theme.
    Mock::VerifyAndClearExpectations(&mock_page_);
    return theme;
  }
};

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetTheme) {
  CustomBackground custom_background;
  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_ntp_custom_background_service_,
          IsCustomBackgroundDisabledByPolicy())
      .WillByDefault(Return(true));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  EXPECT_TRUE(theme->has_background_image);
  EXPECT_TRUE(theme->background_managed_by_policy);
  EXPECT_FALSE(theme->has_third_party_theme);
}

TEST_P(CustomizeChromePageHandlerSetThemeTest, SetThirdPartyTheme) {
  CustomBackground custom_background;

  auto* extension_registry = extensions::ExtensionRegistry::Get(profile_.get());
  scoped_refptr<const extensions::Extension> extension;
  extension = extensions::ExtensionBuilder()
                  .SetManifest(base::DictValue()
                                   .Set("name", "Foo Extension")
                                   .Set("version", "1.0.0")
                                   .Set("manifest_version", 2))
                  .SetID("foo")
                  .Build();
  extension_registry->AddEnabled(extension);

  ON_CALL(mock_ntp_custom_background_service_, GetCustomBackground())
      .WillByDefault(Return(std::make_optional(custom_background)));
  ON_CALL(mock_theme_service(), UsingDefaultTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), UsingExtensionTheme())
      .WillByDefault(Return(true));
  ON_CALL(mock_theme_service(), UsingSystemTheme())
      .WillByDefault(Return(false));
  ON_CALL(mock_theme_service(), GetThemeID()).WillByDefault(Return("foo"));

  side_panel::mojom::ThemePtr theme = UpdateTheme();
  ASSERT_TRUE(theme);
  EXPECT_TRUE(theme->has_background_image);
  EXPECT_TRUE(theme->has_third_party_theme);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CustomizeChromePageHandlerSetThemeTest,
    ::testing::Values(ThemeUpdateSource::kMojo,
                      ThemeUpdateSource::kThemeService,
                      ThemeUpdateSource::kNativeTheme,
                      ThemeUpdateSource::kCustomBackgroundService));

TEST_F(CustomizeChromePageHandlerTest, SetDefaultColor) {
  EXPECT_CALL(mock_theme_service(), UseDefaultTheme).Times(1);

  handler().SetDefaultColor();
}

TEST_F(CustomizeChromePageHandlerTest, RemoveBackgroundImage) {
  EXPECT_CALL(mock_ntp_custom_background_service_, ResetCustomBackgroundInfo)
      .Times(1);

  handler().RemoveBackgroundImage();
}

// Without an extension overriding the new tab page nothing manages it, so no
// page is opened.
TEST_F(CustomizeChromePageHandlerTest, OpenNtpManagedByPage) {
  EXPECT_CALL(mock_open_url_callback_, Run).Times(0);
  handler().OpenNtpManagedByPage();
}

TEST_F(CustomizeChromePageHandlerTest, ScrollToSection) {
  CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(1)
      .WillOnce(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  mock_page_.FlushForTesting();

  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
}

// Ensures that url's are correctly mapped to their NewTabPage type.
// Does not include test for `side_panel::mojom::NewTabPageType::kExtension`
// See `CustomizeChromeInteractiveTest.FooterSectionForExtensionNtp` for
// confirming Customize Chrome shows the footer section for Extension NTPs.
TEST_F(CustomizeChromePageHandlerTest, AttachedTabStateUpdated) {
  std::vector<std::pair<side_panel::mojom::NewTabPageType, GURL>>
      ntp_types_and_urls = {
          {side_panel::mojom::NewTabPageType::kNone,
           GURL("chrome-extension://someinvaldextension/index.html")},
          {side_panel::mojom::NewTabPageType::kThirdPartyWebUI,
           GURL(chrome::kChromeUINewTabPageThirdPartyURL)},
          {side_panel::mojom::NewTabPageType::kNone,
           chrome::ChromeUINewTabURLAsGURL()}};

  for (const auto& ntp_type_and_url : ntp_types_and_urls) {
    side_panel::mojom::NewTabPageType source_tab;
    EXPECT_CALL(mock_page_, AttachedTabStateUpdated)
        .Times(1)
        .WillOnce(SaveArg<0>(&source_tab));
    handler().AttachedTabStateUpdated(ntp_type_and_url.second);
    mock_page_.FlushForTesting();
    EXPECT_EQ(ntp_type_and_url.first, source_tab);
  }
}

TEST_F(CustomizeChromePageHandlerTest, ScrollToUnspecifiedSection) {
  EXPECT_CALL(mock_page_, ScrollToSection).Times(0);

  handler().ScrollToSection(CustomizeChromeSection::kUnspecified);
  mock_page_.FlushForTesting();
}

TEST_F(CustomizeChromePageHandlerTest, UpdateScrollToSection) {
  CustomizeChromeSection section;
  EXPECT_CALL(mock_page_, ScrollToSection)
      .Times(2)
      .WillRepeatedly(SaveArg<0>(&section));

  handler().ScrollToSection(CustomizeChromeSection::kAppearance);
  handler().UpdateScrollToSection();
  mock_page_.FlushForTesting();

  EXPECT_EQ(CustomizeChromeSection::kAppearance, section);
}

class CustomizeChromePageHandlerWithTemplateURLServiceTest
    : public CustomizeChromePageHandlerTest {
 public:
  CustomizeChromePageHandlerWithTemplateURLServiceTest()
      : factory_util_(profile_.get()) {}
  ~CustomizeChromePageHandlerWithTemplateURLServiceTest() override = default;

  void SetUp() override {
    CustomizeChromePageHandlerTest::SetUp();
    factory_util_.VerifyLoad();
    AddSearchProviders();
  }

  static constexpr char16_t kThirdPartyShortName[] = u"third party";
  static constexpr char kThirdPartyDomain[] = "www.third_party.com";

  void AddSearchProviders() {
    {  // third party
      TemplateURLData data;
      data.SetURL(std::string(kThirdPartyDomain) + "/search?q={searchTerms}");
      data.suggestions_url =
          std::string(kThirdPartyDomain) + "/search?q={searchTerms}";
      data.image_url = std::string(kThirdPartyDomain) + "/searchbyimage/upload";
      data.image_translate_url =
          std::string(kThirdPartyDomain) + "/searchbyimage/upload?translate";
      data.new_tab_url = std::string(kThirdPartyDomain) + "/_/chrome/newtab";
      data.contextual_search_url =
          std::string(kThirdPartyDomain) + "/_/contextualsearch";
      data.alternate_urls.push_back(std::string(kThirdPartyDomain) +
                                    "/s#q={searchTerms}");
      data.SetShortName(kThirdPartyShortName);
      third_party_url_ =
          factory_util_.model()->Add(std::make_unique<TemplateURL>(data));
    }
  }

  void SetThirdPartyDefault() {
    factory_util_.model()->SetUserSelectedDefaultSearchProvider(
        third_party_url_);
  }

 private:
  TemplateURLServiceFactoryTestUtil factory_util_;
  raw_ptr<TemplateURL> third_party_url_ = nullptr;
};

// The new tab page is local for every search engine, so a non-Google default
// search engine neither manages it nor classifies a regular tab.
TEST_F(CustomizeChromePageHandlerWithTemplateURLServiceTest,
       NtpNotManagedBySearchEngine) {
  SetThirdPartyDefault();
  mock_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_page_);

  std::string name = "unset";
  std::string description = "unset";
  EXPECT_CALL(mock_page_, NtpManagedByNameUpdated)
      .Times(1)
      .WillOnce(DoAll(SaveArg<0>(&name), SaveArg<1>(&description)));
  handler().UpdateNtpManagedByName();
  mock_page_.FlushForTesting();
  EXPECT_EQ(std::string(), name);
  EXPECT_EQ(std::string(), description);

  side_panel::mojom::NewTabPageType source_tab =
      side_panel::mojom::NewTabPageType::kExtension;
  EXPECT_CALL(mock_page_, AttachedTabStateUpdated)
      .Times(1)
      .WillOnce(SaveArg<0>(&source_tab));
  handler().AttachedTabStateUpdated(GURL("https://example.com"));
  mock_page_.FlushForTesting();
  EXPECT_EQ(side_panel::mojom::NewTabPageType::kNone, source_tab);
}
