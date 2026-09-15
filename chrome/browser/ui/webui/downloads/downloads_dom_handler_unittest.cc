// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/downloads/downloads_dom_handler.h"

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/mock_downloads_page.h"
#include "chrome/test/base/testing_profile.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnRefOfCopy;

class TestDownloadsDOMHandler : public DownloadsDOMHandler {
 public:
  TestDownloadsDOMHandler(mojo::PendingRemote<downloads::mojom::Page> page,
                          content::DownloadManager* download_manager,
                          content::WebUI* web_ui)
      : DownloadsDOMHandler(
            mojo::PendingReceiver<downloads::mojom::PageHandler>(),
            std::move(page),
            download_manager,
            web_ui) {}

  using DownloadsDOMHandler::FinalizeRemovals;
  using DownloadsDOMHandler::RemoveDownloads;
};

}  // namespace

// A fixture to test DownloadsDOMHandler.
class DownloadsDOMHandlerTest : public testing::Test {
 public:
  DownloadsDOMHandlerTest() = default;

  // testing::Test:
  void SetUp() override {
    ON_CALL(manager_, GetBrowserContext())
        .WillByDefault(testing::Return(&profile_));
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    web_ui()->set_web_contents(web_contents_.get());
  }

  TestingProfile* profile() { return &profile_; }
  content::MockDownloadManager* manager() { return &manager_; }
  content::TestWebUI* web_ui() { return &web_ui_; }

 protected:
  testing::StrictMock<MockPage> page_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  // NOTE: The initialization order of these members matters.
  TestingProfile profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;

  testing::NiceMock<content::MockDownloadManager> manager_;
  content::TestWebUI web_ui_;
};

TEST_F(DownloadsDOMHandlerTest, ChecksForRemovedFiles) {
  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  testing::Mock::VerifyAndClear(manager());

  EXPECT_CALL(*manager(), CheckForHistoryFilesRemoval());
}

TEST_F(DownloadsDOMHandlerTest, HandleGetDownloads) {
  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());

  handler.GetDownloads(std::vector<std::string>());

  EXPECT_CALL(page_, InsertItems(0, testing::_));
}

TEST_F(DownloadsDOMHandlerTest, ClearAll) {
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;

  // Safe, in-progress items should be passed over.
  testing::StrictMock<download::MockDownloadItem> in_progress;
  EXPECT_CALL(in_progress, IsDangerous()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, IsInsecure()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, GetDangerType())
      .WillOnce(testing::Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(in_progress, IsTransient()).WillOnce(testing::Return(false));
  EXPECT_CALL(in_progress, GetState())
      .WillOnce(testing::Return(download::DownloadItem::IN_PROGRESS));
  downloads.push_back(&in_progress);

  // Dangerous items should be removed (regardless of state).
  testing::StrictMock<download::MockDownloadItem> dangerous;
  EXPECT_CALL(dangerous, IsDangerous()).WillOnce(testing::Return(true));
  EXPECT_CALL(dangerous, Remove());
  downloads.push_back(&dangerous);

  // Completed items should be marked as hidden from the shelf.
  testing::StrictMock<download::MockDownloadItem> completed;
  EXPECT_CALL(completed, IsDangerous()).WillOnce(testing::Return(false));
  EXPECT_CALL(completed, IsInsecure()).WillOnce(testing::Return(false));
  EXPECT_CALL(completed, GetDangerType())
      .WillOnce(testing::Return(download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS));
  EXPECT_CALL(completed, IsTransient()).WillRepeatedly(testing::Return(false));
  EXPECT_CALL(completed, GetState())
      .WillOnce(testing::Return(download::DownloadItem::COMPLETE));
  EXPECT_CALL(completed, GetId()).WillOnce(testing::Return(1));
  EXPECT_CALL(completed, UpdateObservers());
  downloads.push_back(&completed);

  ASSERT_TRUE(DownloadItemModel(&completed).ShouldShowInUi());

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());
  handler.RemoveDownloads(downloads);

  // Ensure |completed| has been "soft removed" (i.e. can be revived).
  EXPECT_FALSE(DownloadItemModel(&completed).ShouldShowInUi());

  // Make sure |completed| actually get removed when removals are "finalized".
  EXPECT_CALL(*manager(), GetDownload(1)).WillOnce(testing::Return(&completed));
  EXPECT_CALL(completed, Remove());
  handler.FinalizeRemovals();
}

TEST_F(DownloadsDOMHandlerTest, RemoveDownloadsAsyncScanning) {
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;

  testing::StrictMock<download::MockDownloadItem> async_scanning;
  EXPECT_CALL(async_scanning, IsDangerous()).WillOnce(testing::Return(false));
  EXPECT_CALL(async_scanning, IsInsecure()).WillOnce(testing::Return(false));
  EXPECT_CALL(async_scanning, GetDangerType())
      .WillOnce(testing::Return(download::DOWNLOAD_DANGER_TYPE_ASYNC_SCANNING));
  EXPECT_CALL(async_scanning, Remove());
  downloads.push_back(&async_scanning);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());
  handler.RemoveDownloads(downloads);
}

TEST_F(DownloadsDOMHandlerTest, RemoveDownloadsLocalPasswordScanning) {
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;

  testing::StrictMock<download::MockDownloadItem> local_password_scanning;
  EXPECT_CALL(local_password_scanning, IsDangerous())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(local_password_scanning, IsInsecure())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(local_password_scanning, GetDangerType())
      .WillOnce(testing::Return(
          download::DOWNLOAD_DANGER_TYPE_ASYNC_LOCAL_PASSWORD_SCANNING));
  EXPECT_CALL(local_password_scanning, Remove());
  downloads.push_back(&local_password_scanning);

  TestDownloadsDOMHandler handler(page_.BindAndGetRemote(), manager(),
                                  web_ui());
  handler.RemoveDownloads(downloads);
}
