// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/test/tracing/trace_event_analyzer.h"
#include "base/trace_event/trace_config.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/common/api/automation_internal.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/tree_generator.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/display/display_switches.h"


namespace extensions {

namespace {

constexpr char kManifestStub[] = R"(
{
  "name": "chrome.automation.test",
  "key": "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC8xv6iO+j4kzj1HiBL93+XVJH/CRyAQMUHS/Z0l8nCAzaAFkW/JsNwxJqQhrZspnxLqbQxNncXs6g6bsXAwKHiEs+LSs+bIv0Gc/2ycZdhXJ8GhEsSMakog5dpQd1681c2gLK/8CrAoewE/0GIKhaFcp7a2iZlGh4Am6fgMKy0iQIDAQAB",
  "version": "0.1",
  "manifest_version": 2,
  "description": "Tests for the Automation API.",
  "background": { %s },
  "permissions": %s,
  "automation": { "desktop": true }
}
)";

constexpr char kPersistentBackground[] = R"("scripts": ["common.js"])";
constexpr char kServiceWorkerBackground[] = R"("service_worker": "common.js")";
constexpr char kPermissionsDefault[] = R"(["tabs", "http://a.com/"])";

#if !defined(USE_AURA)

constexpr char kPermissionsWindows[] = R"(["windows"])";

#endif

static constexpr char kCommonScript[] = R"(

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;
var StateType = chrome.automation.StateType;

var rootNode = null;
var url = '';

function findAutomationNode(root, condition) {
  if (condition(root)) {
    return root;
  }

  var children = root.children;
  for (var i = 0; i < children.length; i++) {
    var result = findAutomationNode(children[i], condition);
    if (result) {
      return result;
    }
  }
  return null;
}

function runWithDocument(docString, callback) {
  var url = 'data:text/html,<!doctype html>' + docString;
  var createParams = {
    active: true,
    url: url
  };
  createTabAndWaitUntilLoaded(url, function(tab) {
    chrome.automation.getDesktop(desktop => {
      const url = tab.url || tab.pendingUrl;
      let rootNode = desktop.find({attributes: {docUrl: url}});
      if (rootNode && rootNode.docLoaded) {
        callback(rootNode);
        return;
      }

      let listener = () => {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          desktop.removeEventListener('loadComplete', listener);
          desktop.addEventListener('focus', () => {});
          callback(rootNode);
        }
      };
      desktop.addEventListener('loadComplete', listener);
    });
  });
}

function listenOnce(node, eventType, callback, capture) {
  var innerCallback = function(evt) {
    node.removeEventListener(eventType, innerCallback, capture);
    callback(evt);
  };
  node.addEventListener(eventType, innerCallback, capture);
}

function setUpAndRunDesktopTests(allTests) {
  chrome.automation.getDesktop(function(rootNodeArg) {
    rootNode = rootNodeArg;
    chrome.test.runTests(allTests);
  });
}

function setUpAndRunTabsTests(allTests, opt_path, opt_ensurePersists = true) {
  var path = opt_path || 'index.html';
  getUrlFromConfig(path, function(url) {
    createTabAndWaitUntilLoaded(url, function(unused_tab) {
      chrome.automation.getDesktop(function(desktop) {
        rootNode = desktop.find({attributes: {docUrl: url}});
        if (rootNode && rootNode.docLoaded) {
          chrome.test.runTests(allTests);
          return;
        }
        function listener() {
          rootNode = desktop.find({attributes: {docUrl: url}});
          if (rootNode && rootNode.docLoaded) {
            desktop.removeEventListener('loadComplete', listener);
            if (opt_ensurePersists) {
              desktop.addEventListener('focus', () => {});
            }
            chrome.test.runTests(allTests);
          }
        }
        desktop.addEventListener('loadComplete', listener);
      });
    });
  });
}

function getUrlFromConfig(path, callback) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    url = ('http://a.com:PORT/' + path)
        .replace(/PORT/, config.testServer.port);
    callback(url)
  });
}

function createTabAndWaitUntilLoaded(url, callback) {
  chrome.tabs.create({'url': url}, function(tab) {
    chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        callback(tab);
      }
    });
  });
}

async function pollUntil(predicate, pollEveryMs) {
  return new Promise(r => {
    const id = setInterval(() => {
      let ret;
      if (ret = predicate()) {
        clearInterval(id);
        r(ret);
      }
    }, pollEveryMs);
  });
}

const scriptUrl = '_test_resources/api_test/automation/tests/%s';

chrome.test.loadScript(scriptUrl).then(function() {
  // The script will start the tests, so nothing to do here.
}).catch(function(error) {
  chrome.test.fail(scriptUrl + ' failed to load');
});

)";  // kCommonScript

}  // namespace

using ContextType = extensions::browser_test_util::ContextType;

class AutomationApiTest : public ExtensionApiTest {
 public:
  explicit AutomationApiTest(ContextType context_type = ContextType::kNone)
      : ExtensionApiTest(context_type) {}
  ~AutomationApiTest() override = default;
  AutomationApiTest(const AutomationApiTest&) = delete;
  AutomationApiTest& operator=(const AutomationApiTest&) = delete;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }

 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::NumberToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    GURL url =
        embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
    return url;
  }

  void StartEmbeddedTestServer() {
    static const char kSitesDir[] = "automation/sites";
    base::FilePath test_data;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test").AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class AutomationApiTestWithContextType
    : public AutomationApiTest,
      public testing::WithParamInterface<ContextType> {
 public:
  AutomationApiTestWithContextType() : AutomationApiTest(GetParam()) {}
  ~AutomationApiTestWithContextType() override = default;
  AutomationApiTestWithContextType(const AutomationApiTestWithContextType&) =
      delete;
  AutomationApiTestWithContextType& operator=(
      const AutomationApiTestWithContextType&) = delete;

 protected:
  bool CreateExtensionAndRunTest(
      const char* script_path,
      const char* permissions = kPermissionsDefault) {
    TestExtensionDir test_dir;
    const char* background_value = GetParam() == ContextType::kServiceWorker
                                       ? kServiceWorkerBackground
                                       : kPersistentBackground;
    const std::string manifest =
        base::StringPrintf(kManifestStub, background_value, permissions);
    const std::string common_script =
        base::StringPrintf(kCommonScript, script_path);
    test_dir.WriteManifest(manifest);
    test_dir.WriteFile(FILE_PATH_LITERAL("common.js"), common_script);
    return RunExtensionTest(test_dir.UnpackedPath(), {},
                            {.context_type = ContextType::kFromManifest});
  }
};

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutomationApiTestWithContextType,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutomationApiTestWithContextType,
                         ::testing::Values(ContextType::kServiceWorker));

// Canvas tests rely on the harness producing pixel output in order to read back
// pixels from a canvas element. So we have to override the setup function.
class AutomationApiCanvasTest : public AutomationApiTestWithContextType {
 public:
  void SetUp() override {
    EnablePixelOutput();
    AutomationApiTestWithContextType::SetUp();
  }
};

#if defined(USE_AURA)

namespace {
static const char kDomain[] = "a.com";
static const char kGotTree[] = "got_tree";
}  // anonymous namespace

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType,
                       TestRendererAccessibilityEnabled) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsWebContentsOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, ServiceWorker) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsWebContentsOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/service_worker");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsWebContentsOnlyAccessibilityModeForTesting());
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/sanity_check.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, ImageLabels) {
  StartEmbeddedTestServer();
  const GURL url = GetURLForPath(kDomain, "/index.html");
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(), url));

  // Enable image labels.
  profile()->GetPrefs()->SetBoolean(prefs::kAccessibilityImageLabelsEnabled,
                                    true);

  // Initially there should be no accessibility mode set.
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto accessibility_mode = web_contents->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  ASSERT_EQ(ui::AXMode(), accessibility_mode);

  // Enable automation.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  // Now the AXMode should include kLabelImages.
  ui::AXMode expected_mode = ui::kAXModeWebContentsOnly;
  expected_mode.set_mode(ui::AXMode::kLabelImages, true);
  accessibility_mode = web_contents->GetAccessibilityMode();
  // Strip off kNativeAPIs, which may be set in some situations.
  accessibility_mode.set_mode(ui::AXMode::kNativeAPIs, false);
  EXPECT_EQ(expected_mode, accessibility_mode);
}

// Flaky on ChromeOS: crbug.com/375385426
#define MAYBE_Events Events
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_Events) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/events.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Actions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/actions.js")) << message_;
}

// Flaky on ChromeOS: crbug.com/375385426
#define MAYBE_Location Location
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_Location) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/location.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Location2) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/location2.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, BoundsForRange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/bounds_for_range.js"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, LineStartOffsets) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/line_start_offsets.js"))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(PersistentBackground,
                         AutomationApiCanvasTest,
                         ::testing::Values(ContextType::kPersistentBackground));
INSTANTIATE_TEST_SUITE_P(ServiceWorker,
                         AutomationApiCanvasTest,
                         ::testing::Values(ContextType::kServiceWorker));

// Flaky on Mac: crbug.com/1338036
#if BUILDFLAG(IS_MAC)
#define MAYBE_ImageData DISABLED_ImageData
#else
#define MAYBE_ImageData ImageData
#endif
IN_PROC_BROWSER_TEST_P(AutomationApiCanvasTest, MAYBE_ImageData) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/image_data.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, TableProperties) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/table_properties.js"))
      << message_;
}

// Flaky on Mac: crbug.com/40781950
#if BUILDFLAG(IS_MAC)
#define MAYBE_CloseTab DISABLED_CloseTab
#else
#define MAYBE_CloseTab CloseTab
#endif
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_CloseTab) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/close_tab.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Find) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/find.js")) << message_;
}

// Flaky on ChromeOS: crbug.com/375385426
#define MAYBE_Attributes Attributes
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_Attributes) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/attributes.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, ReverseRelations) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/reverse_relations.js"))
      << message_;
}

#define MAYBE_TreeChange TreeChange
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, MAYBE_TreeChange) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/tree_change.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, TreeChangeIndirect) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/tree_change_indirect.js"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, DocumentSelection) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/document_selection.js"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, HitTest) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/hit_test.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, WordBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/word_boundaries.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, SentenceBoundaries) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/sentence_boundaries.js"))
      << message_;
}


IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType,
                       IgnoredNodesNotReturned) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/ignored_nodes_not_returned.js"))
      << message_;
}

// Flaky: crbug.com/335553730
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, DISABLED_ForceLayout) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/force_layout.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, Intents) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/intents.js")) << message_;
}

IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, EnumValidity) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(CreateExtensionAndRunTest("tabs/enum_validity.js")) << message_;
}

#endif  // defined(USE_AURA)

#if !defined(USE_AURA)
IN_PROC_BROWSER_TEST_P(AutomationApiTestWithContextType, DesktopNotSupported) {
  ASSERT_TRUE(CreateExtensionAndRunTest("desktop/desktop_not_supported.js",
                                        kPermissionsWindows))
      << message_;
}
#endif  // !defined(USE_AURA)



}  // namespace extensions
