// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/launch_utils.h"

#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_launch_params.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/types/display_constants.h"

namespace apps {

class LaunchUtilsTest : public testing::Test {
 protected:
  apps::AppLaunchParams CreateLaunchParams(
      apps::LaunchContainer container,
      WindowOpenDisposition disposition,
      bool preferred_container,
      int64_t display_id = display::kInvalidDisplayId,
      apps::LaunchContainer fallback_container =
          apps::LaunchContainer::kLaunchContainerNone) {
    return apps::CreateAppIdLaunchParamsWithEventFlags(
        app_id, apps::GetEventFlags(disposition, preferred_container),
        apps::LaunchSource::kFromChromeInternal, display_id,
        fallback_container);
  }

  std::string app_id = "aaa";
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(LaunchUtilsTest, WindowContainerAndWindowDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerWindow;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndForegoundTabDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, TabContainerAndBackgoundTabDisposition) {
  auto container = apps::LaunchContainer::kLaunchContainerTab;
  auto disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  auto params = CreateLaunchParams(container, disposition, false);

  EXPECT_EQ(container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithTab) {
  auto container = apps::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  auto preferred_container = apps::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true,
                         display::kInvalidDisplayId, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(disposition, params.disposition);
}

TEST_F(LaunchUtilsTest, PreferContainerWithWindow) {
  auto container = apps::LaunchContainer::kLaunchContainerNone;
  auto disposition = WindowOpenDisposition::NEW_WINDOW;
  auto preferred_container = apps::LaunchContainer::kLaunchContainerWindow;
  auto params =
      CreateLaunchParams(container, disposition, true,
                         display::kInvalidDisplayId, preferred_container);

  EXPECT_EQ(preferred_container, params.container);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB, params.disposition);
}

TEST_F(LaunchUtilsTest, UseIntentFullUrlInLaunchParams) {
  auto disposition = WindowOpenDisposition::NEW_WINDOW;

  const GURL url = GURL("https://example.com/?query=1#frag");
  auto intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView, url);

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, apps::GetEventFlags(disposition, true),
      apps::LaunchSource::kFromChromeInternal, display::kInvalidDisplayId,
      apps::LaunchContainer::kLaunchContainerWindow, std::move(intent),
      &profile_);

  EXPECT_EQ(url, params.override_url);
}

TEST_F(LaunchUtilsTest, IntentFilesAreCopiedToLaunchParams) {
  auto disposition = WindowOpenDisposition::NEW_WINDOW;

  std::vector<apps::IntentFilePtr> files;
  std::string file_path = "filesystem:http://foo.com/test/foo.txt";
  auto file = std::make_unique<apps::IntentFile>(GURL(file_path));
  EXPECT_TRUE(file->url.is_valid());
  file->mime_type = "text/plain";
  files.push_back(std::move(file));
  auto intent = std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                               std::move(files));

  auto params = apps::CreateAppLaunchParamsForIntent(
      app_id, apps::GetEventFlags(disposition, true),
      apps::LaunchSource::kFromChromeInternal, display::kInvalidDisplayId,
      apps::LaunchContainer::kLaunchContainerWindow, std::move(intent),
      &profile_);

  ASSERT_EQ(params.launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoAppID) {
  // Validate an empty vector is returned if there is
  // no AppID specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_NoFiles) {
  // Validate an empty vector is returned if there are
  // no files specified on the command line.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(launch_files.size(), 0U);
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_SingleFile) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_MultipleFiles) {
  // Validate a vector with size 2 is returned, and the
  // contents match the command line parameter.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("filename");
  command_line.AppendArg("filename2");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 2U);
  EXPECT_EQ(launch_files[0], base::FilePath(FILE_PATH_LITERAL("filename")));
  EXPECT_EQ(launch_files[1], base::FilePath(FILE_PATH_LITERAL("filename2")));
}

TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_FileProtocol) {
  // Validate a vector with size 1 is returned, and the
  // contents match the command line parameter. This uses
  // the file protocol to reference the file.
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("file://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  ASSERT_EQ(launch_files.size(), 1U);
  EXPECT_EQ(launch_files[0],
            base::FilePath(FILE_PATH_LITERAL("file://filename")));
}

// Verifies that a non-file protocol is not treated as a filename.
TEST_F(LaunchUtilsTest, GetLaunchFilesFromCommandLine_CustomProtocol) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII(switches::kAppId, "test");
  command_line.AppendArg("web+test://filename");
  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  EXPECT_EQ(0U, launch_files.size());
}

}  // namespace apps
