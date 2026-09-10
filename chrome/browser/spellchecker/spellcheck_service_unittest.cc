// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/spellchecker/spellcheck_service.h"

#include <algorithm>
#include <optional>
#include <ostream>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/process/kill.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/supports_user_data.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "components/spellcheck/browser/spellcheck_platform.h"
#include "components/spellcheck/common/spellcheck.mojom.h"
#include "components/spellcheck/common/spellcheck_features.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"

struct TestCase {
  TestCase(
      const std::string& accept_languages,
      const std::vector<std::string>& spellcheck_dictionaries,
      const std::vector<std::string>& expected_languages,
      const std::vector<std::string>& expected_languages_used_for_spellcheck)
      : accept_languages(accept_languages),
        spellcheck_dictionaries(spellcheck_dictionaries) {
    SpellcheckService::Dictionary dictionary;
    for (const auto& language : expected_languages) {
      if (!language.empty()) {
        dictionary.language = language;
        dictionary.used_for_spellcheck = std::ranges::contains(
            expected_languages_used_for_spellcheck, language);
        expected_dictionaries.push_back(dictionary);
      }
    }
  }

  ~TestCase() = default;

  std::string accept_languages;
  std::vector<std::string> spellcheck_dictionaries;
  std::vector<SpellcheckService::Dictionary> expected_dictionaries;
};

bool operator==(const SpellcheckService::Dictionary& lhs,
                const SpellcheckService::Dictionary& rhs) {
  return lhs.language == rhs.language &&
         lhs.used_for_spellcheck == rhs.used_for_spellcheck;
}

std::ostream& operator<<(std::ostream& out,
                         const SpellcheckService::Dictionary& dictionary) {
  out << "{\"" << dictionary.language << "\", used_for_spellcheck="
      << (dictionary.used_for_spellcheck ? "true " : "false") << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out, const TestCase& test_case) {
  out << "language::prefs::kAcceptLanguages=[" << test_case.accept_languages
      << "], prefs::kSpellCheckDictionaries=["
      << base::JoinString(test_case.spellcheck_dictionaries, ",")
      << "], expected=[";
  for (const auto& dictionary : test_case.expected_dictionaries) {
    out << dictionary << ",";
  }
  out << "]";
  return out;
}

static std::unique_ptr<KeyedService> BuildSpellcheckService(
    content::BrowserContext* profile) {
  return std::make_unique<SpellcheckService>(static_cast<Profile*>(profile));
}

class SpellcheckServiceUnitTestBase : public testing::Test {
 public:
  SpellcheckServiceUnitTestBase() = default;

  SpellcheckServiceUnitTestBase(const SpellcheckServiceUnitTestBase&) = delete;
  SpellcheckServiceUnitTestBase& operator=(
      const SpellcheckServiceUnitTestBase&) = delete;

  ~SpellcheckServiceUnitTestBase() override = default;

  content::BrowserContext* browser_context() { return &profile_; }
  PrefService* prefs() { return profile_.GetPrefs(); }

 protected:
  void SetUp() override {
    // Use SetTestingFactoryAndUse to force creation and initialization.
    SpellcheckServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        &profile_, base::BindRepeating(&BuildSpellcheckService));
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

class SpellcheckServiceUnitTest : public SpellcheckServiceUnitTestBase,
                                  public testing::WithParamInterface<TestCase> {
 private:
};

INSTANTIATE_TEST_SUITE_P(
    TestCases,
    SpellcheckServiceUnitTest,
    testing::Values(
        TestCase("en-JP,aa", {"aa"}, {}, {}),
        TestCase("en,aa", {"aa"}, {"en"}, {}),
        TestCase("en,en-JP,fr,aa", {"fr"}, {"en", "fr"}, {"fr"}),
        TestCase("en,en-JP,fr,zz,en-US", {"fr"}, {"en", "fr", "en-US"}, {"fr"}),
        TestCase("en,en-US,en-GB",
                 {"en-GB"},
                 {"en", "en-US", "en-GB"},
                 {"en-GB"}),
        TestCase("en,en-US,en-AU",
                 {"en-AU"},
                 {"en", "en-US", "en-AU"},
                 {"en-AU"}),
        TestCase("en,en-US,en-AU",
                 {"en-US"},
                 {"en", "en-US", "en-AU"},
                 {"en-US"}),
        TestCase("en,en-US", {"en-US"}, {"en", "en-US"}, {"en-US"}),
        TestCase("en,en-US,fr", {"en-US"}, {"en", "en-US", "fr"}, {"en-US"}),
        TestCase("en,fr,en-US,en-AU",
                 {"en-US", "fr"},
                 {"en", "fr", "en-US", "en-AU"},
                 {"fr", "en-US"}),
        TestCase("en-US,en", {"en-US"}, {"en-US", "en"}, {"en-US"}),
        TestCase("hu-HU,hr-HR", {"hr"}, {"hu", "hr"}, {"hr"})));

TEST_P(SpellcheckServiceUnitTest, GetDictionaries) {
  prefs()->SetString(language::prefs::kAcceptLanguages,
                     GetParam().accept_languages);
  base::ListValue spellcheck_dictionaries;
  for (const std::string& dictionary : GetParam().spellcheck_dictionaries) {
    spellcheck_dictionaries.Append(dictionary);
  }
  prefs()->SetList(spellcheck::prefs::kSpellCheckDictionaries,
                   std::move(spellcheck_dictionaries));

  std::vector<SpellcheckService::Dictionary> dictionaries;
  SpellcheckService::GetDictionaries(browser_context(), &dictionaries);

  EXPECT_EQ(GetParam().expected_dictionaries, dictionaries);
}

// Observes the SpellChecker interface for a single renderer. A unit test has no
// real renderers, so the MockRenderProcessHost below is the only host
// InitForAllRenderers() can find, which makes an Initialize() call
// unambiguously attributable to it.
class SpellcheckServiceRendererInitUnitTest
    : public SpellcheckServiceUnitTestBase,
      public spellcheck::mojom::SpellChecker {
 public:
  SpellcheckServiceRendererInitUnitTest() = default;

  SpellcheckServiceRendererInitUnitTest(
      const SpellcheckServiceRendererInitUnitTest&) = delete;
  SpellcheckServiceRendererInitUnitTest& operator=(
      const SpellcheckServiceRendererInitUnitTest&) = delete;

 protected:
  void SetUp() override {
    SpellcheckServiceUnitTestBase::SetUp();
    SpellcheckService::OverrideBinderForTesting(base::BindRepeating(
        &SpellcheckServiceRendererInitUnitTest::Bind, base::Unretained(this)));
    renderer_ = std::make_unique<content::MockRenderProcessHost>(&profile_);
    renderer_->Init();
  }

  void TearDown() override {
    receivers_.Clear();
    renderer_.reset();
    SpellcheckService::OverrideBinderForTesting(base::NullCallback());
  }

  content::MockRenderProcessHost* renderer() { return renderer_.get(); }

  // Waits until at least `count` Initialize() calls have arrived. Returns
  // false on timeout.
  [[nodiscard]] bool WaitForInitializeCount(int count) {
    return base::test::RunUntil(
        [this, count]() { return initialize_count_ >= count; });
  }

 private:
  void Bind(mojo::PendingReceiver<spellcheck::mojom::SpellChecker> receiver) {
    // A ReceiverSet, not a single Receiver: InitForAllRenderers() may reach
    // several hosts, and rebinding a single Receiver would close the earlier
    // pipe before its Initialize() call was delivered, silently losing it.
    receivers_.Add(this, std::move(receiver));
  }

  // spellcheck::mojom::SpellChecker:
  void Initialize(
      std::vector<spellcheck::mojom::SpellCheckBDictLanguagePtr> dictionaries,
      const std::vector<std::string>& custom_words,
      bool enable) override {
    ++initialize_count_;
  }
  void CustomDictionaryChanged(
      const std::vector<std::string>& words_added,
      const std::vector<std::string>& words_removed) override {}

  std::unique_ptr<content::MockRenderProcessHost> renderer_;
  mojo::ReceiverSet<spellcheck::mojom::SpellChecker> receivers_;
  int initialize_count_ = 0;

};

// A renderer that has been initialized but whose process is still launching has
// no process handle yet. It must still be initialized, because
// InitForAllRenderers() is the only path that delivers spellcheck state to a
// renderer that was contacted before its dictionaries were ready.
TEST_F(SpellcheckServiceRendererInitUnitTest, ReachesStillLaunchingRenderer) {
  // Reproduce the window between RenderProcessHostImpl::Init() and
  // OnProcessLaunched().
  renderer()->SimulateProcessStillLaunchingForTesting(true);
  ASSERT_FALSE(renderer()->GetProcess().Handle());
  ASSERT_TRUE(renderer()->IsInitializedAndNotDead());

  // Flipping the pref runs InitForAllRenderers().
  prefs()->SetBoolean(spellcheck::prefs::kSpellCheckEnable, false);

  EXPECT_TRUE(WaitForInitializeCount(1))
      << "a still-launching renderer was never initialized";
}
