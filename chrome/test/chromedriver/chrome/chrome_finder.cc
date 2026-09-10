// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/chrome_finder.h"

#include <stddef.h>

#include <optional>
#include <string>
#include <vector>

#include "base/base_paths.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/chromedriver/constants/version.h"

namespace {

#if BUILDFLAG(IS_LINUX)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  // TODO: Respect users' PATH variables.
  // Until then, we use an approximation of the most common defaults.
  locations->emplace_back("/usr/local/sbin");
  locations->emplace_back("/usr/local/bin");
  locations->emplace_back("/usr/sbin");
  locations->emplace_back("/usr/bin");
  locations->emplace_back("/sbin");
  locations->emplace_back("/bin");
  // Lastly, try the default installation location.
  locations->emplace_back("/opt/google/chrome");
  locations->emplace_back("/opt/chromium.org/chromium");
}
#elif BUILDFLAG(IS_ANDROID)
void GetApplicationDirs(std::vector<base::FilePath>* locations) {
  // On Android we won't be able to find Chrome executable
}
#endif

void GetPathsFromEnvironment(std::vector<base::FilePath>* paths) {
  base::FilePath::StringType delimiter;
  base::FilePath::StringType common_path;
  std::unique_ptr<base::Environment> env(base::Environment::Create());

  std::optional<std::string> path = env->GetVar("PATH");
  if (!path.has_value()) {
    return;
  }

  common_path = path.value();
  delimiter = ":";

  std::vector<base::FilePath::StringType> path_entries = base::SplitString(
      common_path, delimiter, base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (auto& path_entry : path_entries) {
    if (path_entry.size() > 0)
      paths->emplace_back(path_entry);
  }
}

std::vector<base::FilePath> GetChromeProgramNames() {
  return {
#if BUILDFLAG(IS_MAC)
      base::FilePath(chrome::kBrowserProcessExecutablePath),
      base::FilePath(
          chrome::kGoogleChromeForTestingBrowserProcessExecutablePath),
      base::FilePath(chrome::kGoogleChromeBrowserProcessExecutablePath),
      base::FilePath(chrome::kChromiumBrowserProcessExecutablePath),
#elif BUILDFLAG(IS_LINUX)
      base::FilePath(chrome::kBrowserProcessExecutablePath),
      base::FilePath("chrome"),  // Chrome for Testing or Google Chrome
      base::FilePath("google-chrome"),
      base::FilePath("chromium"),
      base::FilePath("chromium-browser"),
#else
    // it will compile but won't work on other OSes
    base::FilePath()
#endif
  };
}

std::vector<base::FilePath> GetHeadlessShellProgramNames() {
  return {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
      base::FilePath("chrome-headless-shell"),
#else
    // it will compile but won't work on other OSes
    base::FilePath()
#endif
  };
}

}  // namespace

namespace internal {

bool FindExe(
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    const std::vector<base::FilePath>& rel_paths,
    const std::vector<base::FilePath>& locations,
    base::FilePath& out_path) {
  for (auto& rel_path : rel_paths) {
    for (auto& location : locations) {
      base::FilePath path = location.Append(rel_path);
      VLOG(logging::LOGGING_INFO) << "Browser search. Trying... " << path;
      if (exists_func.Run(path)) {
        VLOG(logging::LOGGING_INFO) << "Browser search. Found at  " << path;
        out_path = path;
        return true;
      }
    }
  }
  return false;
}

}  // namespace internal

#if BUILDFLAG(IS_MAC)
void GetApplicationDirs(std::vector<base::FilePath>* locations);
#endif

bool FindBrowser(const std::string& browser_name, base::FilePath& browser_exe) {
  return FindBrowser(browser_name, base::BindRepeating(&base::PathExists),
                     browser_exe);
}

/**
 * Finds a browser executable for the provided |browser_name|.
 * For "chrome" each directory is searched in the following flavour priority:
 *   - `PRODUCT_STRING`
 *   - google chrome for testing
 *   - google chrome
 *   - chromium
 * For "chrome-headless-shell" the executable name without extension is always
 * expected to be chrome-headless-shell.
 */
bool FindBrowser(
    const std::string& browser_name,
    const base::RepeatingCallback<bool(const base::FilePath&)>& exists_func,
    base::FilePath& browser_exe) {
  std::vector<base::FilePath> browser_exes;
  if (browser_name == kHeadlessShellCapabilityName) {
    browser_exes = GetHeadlessShellProgramNames();
  } else if (browser_name == kBrowserCapabilityName || browser_name.empty()) {
    // Empty browser_name means that "browserName" capability was not provided.
    // In this case ChromeDriver defaults to "chrome".
    browser_exes = GetChromeProgramNames();
  } else {
    VLOG(logging::LOGGING_ERROR) << "Unknown browser name: " << browser_name;
    return false;
  }

  LOG_IF(ERROR, browser_exes[0].empty()) << "Unsupported platform.";

  base::FilePath module_dir;
  if (base::PathService::Get(base::DIR_MODULE, &module_dir)) {
    for (const base::FilePath& file_path : browser_exes) {
      base::FilePath path = module_dir.Append(file_path);
      VLOG(logging::LOGGING_INFO) << "Browser search. Trying... " << path;
      if (exists_func.Run(path)) {
        browser_exe = path;
        VLOG(logging::LOGGING_INFO) << "Browser search. Found at  " << path;
        return true;
      }
    }
  }

  std::vector<base::FilePath> locations;
  GetApplicationDirs(&locations);
  GetPathsFromEnvironment(&locations);
  bool found =
      internal::FindExe(exists_func, browser_exes, locations, browser_exe);
  if (!found) {
    VLOG(logging::LOGGING_INFO) << "Browser search. Not found.";
  }
  return found;
}
