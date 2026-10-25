// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/utility/safari_data_importer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/bookmarks/test/test_bookmark_client.h"
#include "components/bookmarks/test/test_matchers.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_data_importer/utility/bookmark_parser.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "components/user_data_importer/content/content_bookmark_parser.h"
#include "components/user_data_importer/content/fake_bookmark_html_parser.h"
#include "components/user_data_importer/mojom/bookmark_html_parser.mojom.h"
#include "content/public/test/browser_task_environment.h"  // nogncheck

using bookmarks::test::IsFolder;
using bookmarks::test::IsUrlBookmark;

using testing::_;
using testing::Assign;
using testing::ElementsAre;
using testing::IsEmpty;

namespace user_data_importer {

MATCHER_P(IsOkAndHolds, value, "") {
  return arg.has_value() && arg.value() == value;
}

MATCHER_P(IsError, error, "") {
  return !arg.has_value() && arg.error() == error;
}

class MockSafariDataImportClient : public SafariDataImportClient {
 public:
  MockSafariDataImportClient() = default;
  ~MockSafariDataImportClient() override = default;

  MOCK_METHOD(void, OnTotalFailure, (), (override));
  MOCK_METHOD(void, OnBookmarksReady, (CountOrError), (override));
  MOCK_METHOD(void, OnHistoryReady, (CountOrError), (override));
  MOCK_METHOD(void, OnPaymentCardsReady, (CountOrError), (override));
  MOCK_METHOD(void, OnBookmarksImported, (size_t count), (override));
  MOCK_METHOD(void, OnHistoryImported, (size_t count), (override));
  MOCK_METHOD(void, OnPaymentCardsImported, (size_t count), (override));

  base::WeakPtr<SafariDataImportClient> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSafariDataImportClient> weak_ptr_factory_{this};
};

class SafariDataImporterTest : public testing::Test {
 public:
  SafariDataImporterTest() : html_parser_receiver_{&fake_utility_parser_} {}

  ~SafariDataImporterTest() override = default;

  SafariDataImporterTest(const SafariDataImporterTest&) = delete;
  SafariDataImporterTest& operator=(const SafariDataImporterTest&) = delete;

  TestingPrefServiceSimple pref_service_;

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  void SetUp() override {
    CHECK(history_dir_.CreateUniqueTempDir());
    history_service_ = history::CreateHistoryService(history_dir_.GetPath(),
                                                     /*create_db=*/false);

    auto bookmark_client = std::make_unique<bookmarks::TestBookmarkClient>();

    bookmark_model_ = bookmarks::TestBookmarkClient::CreateModelWithClient(
        std::move(bookmark_client));

    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavingBrowserHistoryDisabled, false);
    pref_service_.registry()->RegisterBooleanPref(
        bookmarks::prefs::kEditBookmarksEnabled, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillCreditCardEnabled, true);

    auto parser = std::make_unique<ContentBookmarkParser>();
    parser->SetServiceForTesting(
        html_parser_receiver_.BindNewPipeAndPassRemote());

    importer_ = std::make_unique<SafariDataImporter>(
        &client_,
        &autofill_client_.GetPersonalDataManager().payments_data_manager(),
        history_service_.get(), bookmark_model_.get(), &pref_service_,
        std::move(parser), "en-US");
  }

  void TearDown() override {
    history_service_->Shutdown();
    Synchronize();
    task_tracker_.TryCancelAll();
  }

  const std::vector<ImportedBookmarkEntry>& GetPendingBookmarks() const {
    return importer_->pending_bookmarks_;
  }

  void PrepareBookmarks(std::string html_data) {
    bookmarks_idle_ = false;
    base::ScopedTempDir dir;
    ASSERT_TRUE(dir.CreateUniqueTempDir());
    base::FilePath path = dir.GetPath().AppendASCII("bookmarks.html");
    ASSERT_TRUE(base::WriteFile(path, html_data));

    importer_->PrepareBookmarks(
        SafariDataImporter::BlockingWorker::BookmarkUnzipResult(
            std::move(path), html_data.length()));
    Synchronize();
  }

  // Executes the import.
  void CompleteImport() {
    importer_->CompleteImport();
    Synchronize();
  }

  void PreparePaymentCards(std::vector<PaymentCardEntry> payment_cards) {
    importer_->PreparePaymentCards(
        SafariDataImporter::BlockingWorker::PaymentCardParseResult(
            std::move(payment_cards), /*size=*/0));
    Synchronize();
  }

  void PrepareInvalidFile() {
    PrepareFile(base::FilePath(FILE_PATH_LITERAL("/invalid/path/to/zip/file")));
  }

  void PrepareImportFromFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    PrepareFile(
        zip_archive_path.Append(FILE_PATH_LITERAL("valid_test_archive.zip")));
  }

  void PrepareImportFromGarbageFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    PrepareFile(
        zip_archive_path.Append(FILE_PATH_LITERAL("garbage_test_archive.zip")));
  }

  void PrepareImportFromNonASCIIFile() {
    base::FilePath zip_archive_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_ASSETS, &zip_archive_path));
    PrepareFile(zip_archive_path.Append(
        base::FilePath(FILE_PATH_LITERAL("non_ascii测试文件.zip"))));
  }

  void CancelImport() { importer_->CancelImport(); }

  void SetHistorySizeThreshold(size_t history_size_threshold) {
    importer_->history_size_threshold_ = history_size_threshold;
  }

  // Sets an expectation of a call to `OnTotalFailure`, and adds the side
  // effect of setting the `bookmarks_idle_` bit.
  void ExpectTotalFailure() {
    EXPECT_CALL(client_, OnTotalFailure())
        .WillOnce(Assign(&bookmarks_idle_, true));
  }

  // Sets an expectation of a call to `OnBookmarksReady`, and adds the side
  // effect of setting the `bookmarks_idle_` bit.
  void ExpectBookmarksReady(auto result, int times = 1) {
    EXPECT_CALL(client_, OnBookmarksReady(result))
        .Times(times)
        .WillRepeatedly(Assign(&bookmarks_idle_, true));
  }

  const bookmarks::BookmarkNode* GetOtherBookmarkNode() {
    return bookmark_model_->other_node();
  }

  // Helper to set up common expectations for the "Ready" phase of an
  // end-to-end file import.
  void SetUpEndToEndPrepareExpectations() {
    ExpectBookmarksReady(IsOkAndHolds(5u));

    EXPECT_CALL(client_, OnPaymentCardsReady(IsOkAndHolds(3u)));
    EXPECT_CALL(client_,
                OnHistoryReady(IsOkAndHolds(13u)));  // Approximation.
  }

  // Helper to set up common expectations for the "Imported" phase of an
  // end-to-end file import.
  void SetUpEndToEndCompleteExpectations() {
    EXPECT_CALL(client_, OnBookmarksImported(5u));
    EXPECT_CALL(client_, OnPaymentCardsImported(3u));
    EXPECT_CALL(client_, OnHistoryImported(7u));  // Actual.
  }

  autofill::TestAutofillClient* autofill_client() { return &autofill_client_; }

  history::HistoryService* history_service() { return history_service_.get(); }

  testing::StrictMock<MockSafariDataImportClient> client_;

  base::CancelableTaskTracker task_tracker_;

 private:
  void PrepareFile(const base::FilePath& file) {
    bookmarks_idle_ = false;
    importer_->PrepareImport(file);
    Synchronize();
  }

  void Synchronize() {
    task_environment_.RunUntilIdle();
  }

  FakeBookmarkHtmlParser fake_utility_parser_;
  mojo::Receiver<user_data_importer::mojom::BookmarkHtmlParser>
      html_parser_receiver_;

  autofill::TestAutofillClient autofill_client_;
  base::ScopedTempDir history_dir_;
  std::unique_ptr<history::HistoryService> history_service_;
  std::unique_ptr<bookmarks::BookmarkModel> bookmark_model_;

  // On iOS, bookmark processing happens in WebKit, so running until idle isn't
  // sufficient; we need to manually track when the parser becomes idle. This is
  // managed by the combination of `ExpectBookmarksReady` and `Synchronize`.
  bool bookmarks_idle_ = true;

  std::unique_ptr<SafariDataImporter> importer_;
};

TEST_F(SafariDataImporterTest, Bookmarks_Basic) {
  ExpectBookmarksReady(IsOkAndHolds(2u));

  PrepareBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>
      </DL>)");

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());
}

// Identical to the above test, but without the top-level <DL> tag enclosing it.
// It's documented as part of the format, but real-world Safari exports don't
// use it, so we have to support both with and without.
TEST_F(SafariDataImporterTest, Bookmarks_NoTopLevelDL) {
  ExpectBookmarksReady(IsOkAndHolds(2u));

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><A HREF="https://www.chromium.org/">Chromium</A>)");

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, IsEmpty());
}

TEST_F(SafariDataImporterTest, Bookmarks_Folders) {
  ExpectBookmarksReady(IsOkAndHolds(3u));

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A HREF="https://www.google.com/" ADD_DATE="904914000">Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.example.com/" ADD_DATE="915181200">Example</A>
        <DT><H3 ADD_DATE="1145523600">Folder 1.1</H3>
        <DL><p>
          <DT><A HREF="https://en.wikipedia.org/wiki/Kitsune" ADD_DATE="1674205200">Kitsune</A>
        </DL><p>
      </DL><p>
      <DT><H3>Empty Folder</H3>
      <DL><p>
      </DL>
      </DL>)");

  ASSERT_EQ(GetPendingBookmarks().size(), 4u);

  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Google");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(904914000));
  EXPECT_EQ(entry.url, GURL("https://www.google.com/"));
  EXPECT_THAT(entry.path, IsEmpty());

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(915181200));
  EXPECT_EQ(entry.url, GURL("https://www.example.com/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[2];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Kitsune");
  EXPECT_EQ(entry.creation_time,
            base::Time::FromSecondsSinceUnixEpoch(1674205200));
  EXPECT_EQ(entry.url, GURL("https://en.wikipedia.org/wiki/Kitsune"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1", u"Folder 1.1"));

  entry = GetPendingBookmarks()[3];
  EXPECT_TRUE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Empty Folder");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_TRUE(entry.url.is_empty());
  EXPECT_THAT(entry.path, IsEmpty());
}

TEST_F(SafariDataImporterTest, Bookmarks_MiscJunk) {
  ExpectBookmarksReady(IsOkAndHolds(2u));

  PrepareBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <!--This is an automatically generated file.
      It will be read and overwritten.
      Do Not Edit! -->
      <DL>
      <DT><A>Google</A>
      <DT><H3>Folder 1</H3>
      <DL><p>
        <DT><A HREF="https://www.chromium.org/">Chromium</A>
        ICON_URI="https://www.chromium.org/favicon.ico"
        <DT><A HREF="https://www.example.org/" ADD_DATE="Last Tuesday">Example</A>
        <DT><A>Google Reader</A>
      </DL><p>
      <!-- Various unsupported types below -->
      FEED="true"
      FEEDURL="https://www.example.com"
      WEBSLICE="true"
      ISLIVEPREVIEW="true"
      PREVIEWSIZE="100 x 100"
      </DL>)");

  ASSERT_EQ(GetPendingBookmarks().size(), 2u);

  // <A>Google</A> was skipped for lack of URL.

  // The folder contains a mix of invalid and valid entries. Ensure the valid
  // ones are preserved.
  ImportedBookmarkEntry entry = GetPendingBookmarks()[0];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Chromium");
  // No timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.chromium.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  entry = GetPendingBookmarks()[1];
  EXPECT_FALSE(entry.is_folder);
  EXPECT_EQ(entry.title, u"Example");
  // Invalid timestamp maps to current time.
  EXPECT_EQ(entry.creation_time, base::Time::Now());
  EXPECT_EQ(entry.url, GURL("https://www.example.org/"));
  EXPECT_THAT(entry.path, ElementsAre(u"Folder 1"));

  // <A>Google Reader</A> was skipped for lack of URL.
}

TEST_F(SafariDataImporterTest, NoPaymentCard) {
  EXPECT_CALL(client_, OnPaymentCardsReady(IsOkAndHolds(0u)));

  PreparePaymentCards(std::vector<PaymentCardEntry>());
}

TEST_F(SafariDataImporterTest, TotalFailure) {
  ExpectTotalFailure();
  PrepareInvalidFile();
}

TEST_F(SafariDataImporterTest, HandleGarbageFile) {
  ExpectBookmarksReady(IsOkAndHolds(0u));
  // History is an approximate count based on the filesize, so we don't know
  // it's garbage yet.
  EXPECT_CALL(client_, OnHistoryReady(IsOkAndHolds(2u)));
  EXPECT_CALL(client_, OnPaymentCardsReady(IsOkAndHolds(0u)));

  PrepareImportFromGarbageFile();

  EXPECT_CALL(client_, OnBookmarksImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  EXPECT_CALL(client_, OnHistoryImported(0));  // Actual.

  CompleteImport();
}

TEST_F(SafariDataImporterTest, CancelImport) {
  ExpectBookmarksReady(_);
  EXPECT_CALL(client_, OnHistoryReady(_));
  EXPECT_CALL(client_, OnPaymentCardsReady(_));

  PrepareImportFromFile();

  // No additional calls to the client are made after a cancellation, since
  // nothing is ultimately imported.
  CancelImport();
}

TEST_F(SafariDataImporterTest, ImportFileEndToEnd) {
  SetUpEndToEndPrepareExpectations();
  SetUpEndToEndCompleteExpectations();

  PrepareImportFromFile();

  // Use a small history size threshold so that ParseHistoryCallback gets called
  // multiple times internally.
  SetHistorySizeThreshold(3u);
  CompleteImport();
}

TEST_F(SafariDataImporterTest, ImportFileEndToEndWithDefaultThreshold) {
  SetUpEndToEndPrepareExpectations();
  SetUpEndToEndCompleteExpectations();

  PrepareImportFromFile();
  CompleteImport();
}

// Smoke test to make sure that PrepareImport is idempotent(ish).
TEST_F(SafariDataImporterTest, PrepareImportFileTwice) {
  // Despite running twice, the results should be identical both times.
  ExpectBookmarksReady(IsOkAndHolds(5u), /*times=*/2);

  EXPECT_CALL(client_, OnPaymentCardsReady(IsOkAndHolds(3u))).Times(2);
  EXPECT_CALL(client_, OnHistoryReady(IsOkAndHolds(13u))).Times(2);

  PrepareImportFromFile();
  PrepareImportFromFile();
}

// Tests importing a single bookmark into the "Imported from Safari" folder.
TEST_F(SafariDataImporterTest, ImportSingleBookmark) {
  ExpectBookmarksReady(IsOkAndHolds(1u));
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DT><A HREF="https://www.example.com/">Single Bookmark</A>)");

  EXPECT_CALL(client_, OnBookmarksImported(1u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0u));

  CompleteImport();

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(),
              ElementsAre(IsFolder(
                  u"Imported from Safari",
                  ElementsAre(IsUrlBookmark(
                      u"Single Bookmark", GURL("https://www.example.com/"))))));
}

// Tests importing multiple bookmarks into the "Imported from Safari" folder.
TEST_F(SafariDataImporterTest, ImportsMultipleBookmarks) {
  ExpectBookmarksReady(IsOkAndHolds(2u));
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><A HREF="https://www.one.com/">First Bookmark</A>
            <DT><A HREF="https://www.two.com/">Second Bookmark</A>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(2u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport();

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(other_node->children(),
              ElementsAre(IsFolder(
                  u"Imported from Safari",
                  ElementsAre(IsUrlBookmark(u"First Bookmark",
                                            GURL("https://www.one.com/")),
                              IsUrlBookmark(u"Second Bookmark",
                                            GURL("https://www.two.com/"))))));
}

// Tests that the folder hierarchy is preserved when importing a nested
// bookmark.
TEST_F(SafariDataImporterTest, ImportsNestedBookmark) {
  ExpectBookmarksReady(IsOkAndHolds(1u));
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Top Folder</H3>
            <DL>
              <DT><H3>Second Folder</H3>
                <DL>
                  <DT><A HREF="https://www.nested.com/">Nested Bookmark</A>
                </DL>
            </DL>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(1u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport();

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(
          u"Imported from Safari",
          ElementsAre(IsFolder(
              u"Top Folder",
              ElementsAre(IsFolder(u"Second Folder",
                                   ElementsAre(IsUrlBookmark(
                                       u"Nested Bookmark",
                                       GURL("https://www.nested.com/"))))))))));
}

// Tests that an empty bookmark folder is imported correctly.
TEST_F(SafariDataImporterTest, ImportsEmptyFolder) {
  ExpectBookmarksReady(IsOkAndHolds(0u));
  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Empty Folder</H3>
            <DL></DL>
          </DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(0u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport();

  const bookmarks::BookmarkNode* other_node = GetOtherBookmarkNode();
  EXPECT_THAT(
      other_node->children(),
      ElementsAre(IsFolder(u"Imported from Safari",
                           ElementsAre(IsFolder(u"Empty Folder", IsEmpty())))));
}

TEST_F(SafariDataImporterTest, DuplicateBookmarkFolders) {
// TODO(crbug.com/407587751): Align behaviour of ContentBookmarkParser and
// IOSBookmarkParser.
  ExpectBookmarksReady(IsOkAndHolds(2u));

  PrepareBookmarks(
      R"(<!DOCTYPE NETSCAPE-Bookmark-file-1>
          <DL>
            <DT><H3>Folder A</H3>
            <DL>
              <DT><A HREF="https://www.example1.com/">Bookmark 1</A>
            </DL>
            <DT><H3>Folder A</H3> <DL>
              <DT><H3>Folder B</H3>
              <DL>
                <DT><A HREF="https://www.example2.com/">Bookmark 2</A>
              </DL>
            </DL>
            <DT><H3>Folder A</H3> <DL>
              <DT><A HREF="https://www.example3.com/">Bookmark 3</A>
            </DL>
          </DL>)");

// TODO(crbug.com/407587751): Align behaviour of ContentBookmarkParser and
// IOSBookmarkParser.
  EXPECT_CALL(client_, OnBookmarksImported(2u));

  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport();

  const bookmarks::BookmarkNode* import_folder =
      GetOtherBookmarkNode()->children().at(0).get();

  EXPECT_THAT(
      import_folder->children(),
      ElementsAre(
          IsFolder(u"Folder A",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 1", GURL("https://www.example1.com/")))),
          IsFolder(u"Folder B",
                   ElementsAre(IsUrlBookmark(
                       u"Bookmark 2", GURL("https://www.example2.com/"))))));
}

// Tests that an empty "Imported from Safari" folder is not created when there
// are no bookmarks to import.
TEST_F(SafariDataImporterTest, ImportWithNoBookmarks) {
  ExpectBookmarksReady(IsOkAndHolds(0u));
  PrepareBookmarks("");

  EXPECT_CALL(client_, OnBookmarksImported(0u));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));
  CompleteImport();

  EXPECT_THAT(GetOtherBookmarkNode()->children(), IsEmpty());
}

// Tests that history import is blocked when disabled by enterprise policy.
TEST_F(SafariDataImporterTest, ImportHistoryBlockedByPolicy) {
  pref_service_.SetManagedPref(prefs::kSavingBrowserHistoryDisabled,
                               std::make_unique<base::Value>(true));

  ExpectBookmarksReady(IsOkAndHolds(5u));
  EXPECT_CALL(client_, OnPaymentCardsReady(IsOkAndHolds(3u)));
  EXPECT_CALL(client_, OnHistoryReady(
                           IsError(ImportPreparationError::kBlockedByPolicy)));

  PrepareImportFromFile();

  EXPECT_CALL(client_, OnBookmarksImported(5u));
  EXPECT_CALL(client_, OnPaymentCardsImported(3u));
  EXPECT_CALL(client_, OnHistoryImported(0));

  CompleteImport();

  // TODO(crbug.com/407587751): Move this into a helper method to be
  // reused by other tests.
  history::QueryResults results;
  history_service()->QueryHistory(
      u"", history::QueryOptions(),
      base::BindLambdaForTesting(
          [&](history::QueryResults r) { results = std::move(r); }),
      &task_tracker_);
  history::BlockUntilHistoryProcessesPendingRequests(history_service());

  EXPECT_TRUE(results.empty());
}

// Tests that bookmark import is blocked when disabled by enterprise policy.
TEST_F(SafariDataImporterTest, ImportBookmarksBlockedByPolicy) {
  pref_service_.SetManagedPref(bookmarks::prefs::kEditBookmarksEnabled,
                               std::make_unique<base::Value>(false));

  ExpectBookmarksReady(IsError(ImportPreparationError::kBlockedByPolicy));
  PrepareBookmarks(R"(
      <!DOCTYPE NETSCAPE-Bookmark-file-1>
      <DL><DT><A HREF="https://www.google.com/">Google</A></DL>)");

  EXPECT_CALL(client_, OnBookmarksImported(0));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));

  CompleteImport();

  EXPECT_THAT(GetOtherBookmarkNode()->children(), IsEmpty());
}

// Tests that payment card import is blocked when disabled by enterprise policy.
TEST_F(SafariDataImporterTest, ImportPaymentCardsBlockedByPolicy) {
  pref_service_.SetManagedPref(autofill::prefs::kAutofillCreditCardEnabled,
                               std::make_unique<base::Value>(false));

  EXPECT_CALL(client_, OnPaymentCardsReady(
                           IsError(ImportPreparationError::kBlockedByPolicy)));
  PreparePaymentCards({PaymentCardEntry{.card_number = u"1234567812345678",
                                        .card_name = u"Test Card",
                                        .cardholder_name = "Test Name",
                                        .card_expiration_month = 12,
                                        .card_expiration_year = 2025}});

  EXPECT_CALL(client_, OnBookmarksImported(0));
  EXPECT_CALL(client_, OnHistoryImported(0));
  EXPECT_CALL(client_, OnPaymentCardsImported(0));

  CompleteImport();

  EXPECT_TRUE(autofill_client()
                  ->GetPersonalDataManager()
                  .payments_data_manager()
                  .GetCreditCards()
                  .empty());
}

}  // namespace user_data_importer
