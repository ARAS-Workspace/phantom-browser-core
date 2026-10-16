// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/new_tab_page/untrusted_source.h"

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class UntrustedSourceTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = MakeTestingProfile();
    untrusted_source_ = std::make_unique<UntrustedSource>(profile_.get());
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    test_web_contents_getter_ =
        base::BindLambdaForTesting([&] { return test_web_contents_.get(); });
  }

  void TearDown() override {
    untrusted_source_.reset();
    test_web_contents_.reset();
    test_web_contents_getter_ = content::WebContents::Getter();
    profile_.reset();
  }

  std::unique_ptr<TestingProfile> MakeTestingProfile() {
    return TestingProfile::Builder().Build();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<UntrustedSource> untrusted_source_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  content::WebContents::Getter test_web_contents_getter_;
};

TEST_F(UntrustedSourceTest, BackgroundRequest_Valid) {
  static constexpr uint8_t kData[] = {1, 2, 3, 4};
  ASSERT_TRUE(base::WriteFile(profile_->GetPath().AppendASCII("background.jpg"),
                              kData));

  {
    base::RunLoop run_loop;
    base::MockCallback<content::URLDataSource::GotDataCallback> callback;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
          EXPECT_EQ(base::span(kData), base::span(*memory));
          run_loop.Quit();
        });

    untrusted_source_->StartDataRequest(
        GURL("chrome-untrusted://new-tab-page/background.jpg"),
        test_web_contents_getter_, callback.Get());
    run_loop.Run();
  }

  // Verify that a valid token-prefixed URL also works.
  static constexpr char kToken[] = "1234567890ABCDEF1234567890ABCDEF";
  base::FilePath token_background_img =
      profile_->GetPath().AppendASCII(std::string(kToken) + "background.jpg");
  static constexpr uint8_t kTokenData[] = {5, 6, 7, 8};
  ASSERT_TRUE(base::WriteFile(token_background_img, kTokenData));

  {
    base::RunLoop run_loop;
    base::MockCallback<content::URLDataSource::GotDataCallback> callback;
    EXPECT_CALL(callback, Run(testing::_))
        .Times(1)
        .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
          EXPECT_EQ(base::span(kTokenData), base::span(*memory));
          run_loop.Quit();
        });

    untrusted_source_->StartDataRequest(
        GURL(std::string("chrome-untrusted://new-tab-page/") + kToken +
             "background.jpg"),
        test_web_contents_getter_, callback.Get());
    run_loop.Run();
  }
}

// Verifies that directory traversal is disallowed (crbug.com/497241148).
TEST_F(UntrustedSourceTest, BackgroundRequest_DirectoryTraversal) {
  base::FilePath extensions_dir =
      profile_->GetPath().AppendASCII("12345678901234567890123456789012");
  ASSERT_TRUE(base::CreateDirectory(extensions_dir));
  base::FilePath background_img = extensions_dir.AppendASCII("background.jpg");
  static constexpr uint8_t kData[] = {1, 2, 3, 4};
  ASSERT_TRUE(base::WriteFile(background_img, kData));

  base::RunLoop run_loop;
  base::MockCallback<content::URLDataSource::GotDataCallback> callback;
  EXPECT_CALL(callback, Run(testing::_))
      .Times(1)
      .WillOnce([&](scoped_refptr<base::RefCountedMemory> memory) {
        EXPECT_TRUE(base::span(*memory).empty());
        run_loop.Quit();
      });

  untrusted_source_->StartDataRequest(
      GURL("chrome-untrusted://new-tab-page/12345678901234567890123456789012/"
           "background.jpg"),
      test_web_contents_getter_, callback.Get());
  run_loop.Run();
}
