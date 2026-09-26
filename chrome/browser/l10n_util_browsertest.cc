// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/l10n_util.h"

#include <string>
#include <vector>

#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using ::testing::Optional;
using ::testing::StrEq;

class L10nUtilBrowserTest : public InProcessBrowserTest {
 public:
  L10nUtilBrowserTest() = default;
  ~L10nUtilBrowserTest() override = default;
  L10nUtilBrowserTest(const L10nUtilBrowserTest&) = delete;
  L10nUtilBrowserTest& operator=(const L10nUtilBrowserTest&) = delete;
};

}  // namespace

// Tests whether CheckAndResolveLocale returns the same result with and without
// I/O.
IN_PROC_BROWSER_TEST_F(L10nUtilBrowserTest, CheckAndResolveLocaleIO) {
  base::ScopedAllowBlockingForTesting allow_io;
  std::vector<std::string> accept_languages;
  l10n_util::GetAcceptLanguages(&accept_languages);

  for (const std::string& locale : accept_languages) {
    const std::optional<std::string> resolved_locale =
        l10n_util::CheckAndResolveLocale(
            locale, l10n_util::CheckLocaleMode::kUseKnownLocalesList);
    const std::optional<std::string> resolved_locale_with_io =
        l10n_util::CheckAndResolveLocale(
            locale, l10n_util::CheckLocaleMode::kVerifyLocalizationDataExists);

    // On other platforms, the two function calls should be identical.
    EXPECT_EQ(resolved_locale, resolved_locale_with_io);
  }
}
