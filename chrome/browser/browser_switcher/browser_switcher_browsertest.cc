// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_service.h"

#include <string.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace browser_switcher {

namespace {

// A URL that should trigger a switch.
const char kTestUrl[] = "http://example.com/foobar";
const char kTestUrlWithSpaces[] = "http://example.com/foobar baz";

// A URL that shouldn't trigger a switch.
const char kOtherUrl[] = "http://google.com/";

// |echo| adds a newline at the end of the file. CRLF on Windows, but just LF on
// POSIX systems.
const char kTestUrlWithLineEnding[] = "http://example.com/foobar\n";

std::string NativeToUTF8(const std::string& native) {
  return native;
}

void SetPolicy(policy::PolicyMap* map,
               const std::string& policy_name,
               base::Value value) {
  map->Set(policy_name, policy::POLICY_LEVEL_MANDATORY,
           policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_PLATFORM,
           std::move(value), nullptr);
}

// Sets the following policies, in the Chrome policy domain:
// - AlternativeBrowserPath: cmd_line's command
// - AlternativeBrowserParameters: cmd_line's parameters
// - BrowserSwitcherUrlList: ["example.com"]
void InitPolicies(policy::MockConfigurationPolicyProvider* provider,
                  const base::CommandLine& cmd_line) {
  policy::PolicyMap map;

  SetPolicy(&map, policy::key::kBrowserSwitcherEnabled, base::Value(true));
  SetPolicy(&map, policy::key::kAlternativeBrowserPath,
            base::Value(cmd_line.GetProgram().MaybeAsASCII()));

  base::ListValue params;
  for (size_t i = 1; i < cmd_line.argv().size(); i++)
    params.Append(NativeToUTF8(cmd_line.argv()[i]));
  SetPolicy(&map, policy::key::kAlternativeBrowserParameters,
            base::Value(std::move(params)));

  base::ListValue sitelist;
  sitelist.Append("example.com");
  SetPolicy(&map, policy::key::kBrowserSwitcherUrlList,
            base::Value(std::move(sitelist)));

  provider->UpdateChromePolicy(map);
  base::RunLoop().RunUntilIdle();
}

// Generates a command-line that prints the URL to a file when run. Make sure
// the navigation URL and |output_file| don't contain any special characters or
// whitespace.
base::CommandLine GenerateEchoCommandLine(const base::FilePath& output_file) {
  // bin/sh -c 'echo "${url}"> "output_file"'
  std::vector<std::string> args = {
      "/bin/sh", "-c",
      base::StringPrintf("echo \"${url}\" > \"%s\"",
                         output_file.MaybeAsASCII().c_str()),
  };
  return base::CommandLine(std::move(args));
}

}  // namespace

class BrowserSwitcherBrowserTest : public InProcessBrowserTest {
 public:
  BrowserSwitcherBrowserTest() = default;
  ~BrowserSwitcherBrowserTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    InProcessBrowserTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(&provider_);
  }

  policy::MockConfigurationPolicyProvider* provider() { return &provider_; }
  base::FilePath GetTempDir() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> provider_;
};

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, RunsExternalCommand) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("RunsExternalCommand.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kTestUrl), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();

  // File content should be equal to the navigated URL.
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file, &output));
  EXPECT_EQ(std::string(kTestUrlWithLineEnding), output);
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, DoesNotKeepSpaces) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("RunsExternalCommand.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kTestUrlWithSpaces),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();

  // Check that there's no space in the URL (i.e. replaced with %20).
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string output;
  ASSERT_TRUE(base::ReadFileToString(temp_file, &output));
  EXPECT_FALSE(output.contains(' '));
  EXPECT_TRUE(output.contains("%20"));
}

IN_PROC_BROWSER_TEST_F(BrowserSwitcherBrowserTest, DoesNotRunOnRandomUrls) {
  base::FilePath temp_file =
      GetTempDir().AppendASCII("DoesNotRunOnRandomUrls.txt");
  base::CommandLine cmd_line = GenerateEchoCommandLine(temp_file);

  InitPolicies(provider(), cmd_line);

  // We open a new tab, because closing the last tab in the browser
  // causes the whole browser to close.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(kOtherUrl), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NO_WAIT);

  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
  run_loop.Run();

  // Check that a random URL didn't cause a "browser switch" to trigger.
  base::ScopedAllowBlockingForTesting allow_blocking;
  EXPECT_FALSE(base::PathExists(temp_file));
}

}  // namespace browser_switcher
