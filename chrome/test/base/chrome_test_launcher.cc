// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chrome_test_launcher.h"

#include <memory>
#include <string_view>
#include <utility>
#include <variant>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/profiler/thread_group_profiler.h"
#include "base/run_loop.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/strings/string_util.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/test/test_switches.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/profiler/chrome_thread_group_profiler_client.h"
#include "chrome/common/profiler/chrome_thread_profiler_client.h"
#include "chrome/common/profiler/main_thread_stack_sampling_profiler.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/taskbar_util.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/test/base/chrome_test_suite.h"
#include "chrome/utility/chrome_content_utility_client.h"
#include "components/crash/core/app/crashpad.h"
#include "components/sampling_profiler/thread_profiler.h"
#include "content/public/app/content_main.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/test/network_service_test_helper.h"
#include "content/public/test/test_launcher.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/service_factory.h"
#include "services/test/echo/echo_service.h"
#include "testing/libfuzzer/fuzztest_init_helper.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_testing_support.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/bundle_locations.h"
#include "chrome/browser/chrome_browser_application_mac.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "chrome/app/chrome_crash_reporter_client.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/first_run/scoped_relaunch_chrome_browser_override.h"
#include "chrome/browser/upgrade_detector/installed_version_poller.h"
#include "testing/gtest/include/gtest/gtest.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

// static
int ChromeTestSuiteRunner::RunTestSuiteInternal(ChromeTestSuite* test_suite) {
  // Browser tests are expected not to tear-down various globals.
  test_suite->DisableCheckForLeakedGlobals();
#if BUILDFLAG(IS_ANDROID)
  // Android browser tests run child processes as threads instead.
  content::ContentTestSuiteBase::RegisterInProcessThreads();
#endif
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  InstalledVersionPoller::ScopedDisableForTesting disable_polling(
      InstalledVersionPoller::MakeScopedDisableForTesting());
#endif
  return test_suite->Run();
}

int ChromeTestSuiteRunner::RunTestSuite(int argc, char** argv) {
  ChromeTestSuite test_suite(argc, argv);
  return RunTestSuiteInternal(&test_suite);
}

namespace {

auto RunEchoService(mojo::PendingReceiver<echo::mojom::EchoService> receiver) {
  return std::make_unique<echo::EchoService>(std::move(receiver));
}

}  // namespace

ChromeTestLauncherDelegate::ChromeTestLauncherDelegate(
    ChromeTestSuiteRunner* runner)
    : runner_(runner) {}
ChromeTestLauncherDelegate::~ChromeTestLauncherDelegate() = default;

int ChromeTestLauncherDelegate::RunTestSuite(int argc, char** argv) {
  return runner_->RunTestSuite(argc, argv);
}

std::string
ChromeTestLauncherDelegate::GetUserDataDirectoryCommandLineSwitch() {
  return switches::kUserDataDir;
}

namespace {

// Frame observer that injects `window.internals` JavaScript object whenever a
// window object is cleared, if `--expose-internals-for-testing` is enabled.
// Self-deletes via `OnDestruct()` when the associated `RenderFrame` is
// destroyed.
class InternalsObjectFrameInjector : public content::RenderFrameObserver {
 public:
  explicit InternalsObjectFrameInjector(content::RenderFrame* render_frame)
      : content::RenderFrameObserver(render_frame) {}
  void DidClearWindowObject() override {
    if (render_frame() && render_frame()->GetWebFrame()) {
      blink::WebTestingSupport::InjectInternalsObject(
          render_frame()->GetWebFrame());
    }
  }
  void OnDestruct() override { delete this; }
};

// A replacement ChromeContentRendererClient for browser tests that hooks frame
// creation to inject test-only bindings like `window.internals` without
// linking test-only dependencies into production Chrome renderer code.
class BrowserTestChromeContentRendererClient
    : public ChromeContentRendererClient {
 public:
  void RenderFrameCreated(content::RenderFrame* render_frame) override {
    ChromeContentRendererClient::RenderFrameCreated(render_frame);
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kExposeInternalsForTesting)) {
      new InternalsObjectFrameInjector(render_frame);
    }
  }
};

}  // namespace

#if BUILDFLAG(IS_ANDROID)
ChromeTestChromeMainDelegate::ChromeTestChromeMainDelegate() = default;
#else
ChromeTestChromeMainDelegate::ChromeTestChromeMainDelegate()
    : ChromeMainDelegate({.exe_entry_point_ticks = base::TimeTicks::Now()}) {}
#endif

ChromeTestChromeMainDelegate::~ChromeTestChromeMainDelegate() = default;

content::ContentRendererClient*
ChromeTestChromeMainDelegate::CreateContentRendererClient() {
  chrome_content_renderer_client_ =
      std::make_unique<BrowserTestChromeContentRendererClient>();
  return chrome_content_renderer_client_.get();
}

// A replacement ChromeContentUtilityClient that binds the
// echo::mojom::EchoService within the Utility process. For use with testing
// only.
class BrowserTestChromeContentUtilityClient
    : public ChromeContentUtilityClient {
 public:
  void RegisterIOThreadServices(mojo::ServiceFactory& services) override {
    ChromeContentUtilityClient::RegisterIOThreadServices(services);
    services.Add(RunEchoService);
  }
};

content::ContentUtilityClient*
ChromeTestChromeMainDelegate::CreateContentUtilityClient() {
  chrome_content_utility_client_ =
      std::make_unique<BrowserTestChromeContentUtilityClient>();
  return chrome_content_utility_client_.get();
}

std::optional<int> ChromeTestChromeMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  auto result = ChromeMainDelegate::PostEarlyInitialization(invoked_in);
  if (std::get_if<InvokedInBrowserProcess>(&invoked_in)) {
    // If servicing an `InProcessBrowserTest`, give the test an opportunity to
    // prepopulate Local State with preferences.
    ChromeFeatureListCreator* chrome_feature_list_creator =
        chrome_content_browser_client_->startup_data()
            ->chrome_feature_list_creator();
    PrefService* const local_state = chrome_feature_list_creator->local_state();
#if BUILDFLAG(IS_ANDROID)
    if (auto* test_instance = AndroidBrowserTest::GetCurrent()) {
      test_instance->SetUpLocalStatePrefService(local_state);
    }
#else
    if (auto* test_instance = InProcessBrowserTest::GetCurrent()) {
      test_instance->SetUpLocalStatePrefService(local_state);
    }
#endif
  }
  return result;
}

void ChromeTestChromeMainDelegate::CreateThreadPool(std::string_view name) {
  // The ThreadGroupProfiler client must be set before thread pool is
  // created (below).
  base::ThreadGroupProfiler::SetClient(
      std::make_unique<ChromeThreadGroupProfilerClient>());

  base::test::TaskEnvironment::CreateThreadPool();

  // The ThreadProfiler client must be set before main thread profiling is
  // started (below).
  sampling_profiler::ThreadProfiler::SetClient(
      std::make_unique<ChromeThreadProfilerClient>());

// `ChromeMainDelegateAndroid::PreSandboxStartup` creates the profiler a little
// later.
#if !BUILDFLAG(IS_ANDROID)
  // Start the sampling profiler as early as possible - namely, once the thread
  // pool has been created.
  sampling_profiler_ = std::make_unique<MainThreadStackSamplingProfiler>();
#endif
}

bool ChromeTestChromeMainDelegate::IsInitFeatureListEarly() {
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
content::ContentMainDelegate*
ChromeTestLauncherDelegate::CreateContentMainDelegate() {
  return new ChromeTestChromeMainDelegate();
}
#endif

void ChromeTestLauncherDelegate::PreSharding() {
}

void ChromeTestLauncherDelegate::OnDoneRunningTests() {
}

int LaunchChromeTests(size_t parallel_jobs,
                      content::TestLauncherDelegate* delegate,
                      int argc,
                      char** argv) {
  base::test::AllowCheckIsTestForTesting();

#if BUILDFLAG(IS_MAC)
  // Set up the path to the framework so resources can be loaded. This is also
  // performed in ChromeTestSuite, but in browser tests that only affects the
  // browser process. Child processes need access to the Framework bundle too.
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_EXE, &path));
  path = path.Append(chrome::kFrameworkName);
  base::apple::SetOverrideFrameworkBundlePath(path);
#endif

  // PoissonAllocationSampler's TLS slots need to be set up before
  // MainThreadStackSamplingProfiler, which can allocate TLS slots of its own.
  // On some platforms pthreads can malloc internally to access higher-numbered
  // TLS slots, which can cause reentry in the heap profiler. (See the comment
  // on ReentryGuard::InitTLSSlot().)
  // TODO(crbug.com/40062835): Clean up other paths that call this Init()
  // function, which are now redundant.
  base::PoissonAllocationSampler::Init();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  ChromeCrashReporterClient::Create();
#endif

  // Setup a working test environment for the network service in case it's used.
  // Only create this object in the utility process, so that its members don't
  // interfere with other test objects in the browser process.
  std::unique_ptr<content::NetworkServiceTestHelper>
      network_service_test_helper = content::NetworkServiceTestHelper::Create();

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  // Cause a test failure for any test that triggers an unexpected relaunch.
  // Tests that fail here should likely be restructured to put the "before
  // relaunch" code into a PRE_ test with its own
  // ScopedRelaunchChromeBrowserOverride and the "after relaunch" code into the
  // normal non-PRE_ test.
  upgrade_util::ScopedRelaunchChromeBrowserOverride fail_on_relaunch(
      base::BindRepeating([](const base::CommandLine&) {
        ADD_FAILURE() << "Unexpected call to RelaunchChromeBrowser";
        return false;
      }));
#endif

  // This is needed because when running the browser test in multi-process
  // mode, the FuzzTest initialization code will not get called in the child
  // process.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          "single-process-tests")) {
    MaybeInitFuzztest(argc, argv);
  }

  return content::LaunchTests(delegate, parallel_jobs, argc, argv);
}
