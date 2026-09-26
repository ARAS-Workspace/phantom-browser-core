// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "base/trace_event/trace_config_memory_test_util.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::trace_event::MemoryDumpArgs;
using base::trace_event::MemoryDumpDeterminism;
using base::trace_event::MemoryDumpLevelOfDetail;
using base::trace_event::MemoryDumpManager;
using base::trace_event::MemoryDumpType;
using base::trace_event::ProcessMemoryDump;
using memory_instrumentation::mojom::RequestOutcome;
using testing::_;
using testing::Return;

namespace content {

// A mock dump provider, used to check that dump requests actually end up
// creating memory dumps.
class MockDumpProvider : public base::trace_event::MemoryDumpProvider {
 public:
  MOCK_METHOD2(OnMemoryDump, bool(const MemoryDumpArgs& args,
                                  ProcessMemoryDump* pmd));
};

class MemoryTracingTest : public ContentBrowserTest {
 public:
  // Used as callback argument for MemoryDumpManager::RequestGlobalDump():
  void OnGlobalMemoryDumpDone(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure closure,
      uint32_t request_index,
      RequestOutcome outcome,
      uint64_t dump_guid) {
    // Make sure we run the RunLoop closure on the same thread that originated
    // the run loop (which is the IN_PROC_BROWSER_TEST_F main thread).
    if (!task_runner->RunsTasksInCurrentSequence()) {
      task_runner->PostTask(
          FROM_HERE, base::BindOnce(&MemoryTracingTest::OnGlobalMemoryDumpDone,
                                    base::Unretained(this), task_runner,
                                    std::move(closure), request_index, outcome,
                                    dump_guid));
      return;
    }
    if (outcome == RequestOutcome::kSuccess) {
      EXPECT_NE(0u, dump_guid);
    }
    OnMemoryDumpDone(request_index, outcome);
    if (closure)
      std::move(closure).Run();
  }

  void RequestGlobalDumpWithClosure(
      bool from_renderer_thread,
      const MemoryDumpType& dump_type,
      const MemoryDumpLevelOfDetail& level_of_detail,
      base::OnceClosure closure) {
    uint32_t request_index = next_request_index_++;
    auto callback = base::BindOnce(
        &MemoryTracingTest::OnGlobalMemoryDumpDone, base::Unretained(this),
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(closure),
        request_index);
    if (from_renderer_thread) {
      PostTaskToInProcessRendererAndWait(base::BindOnce(
          &memory_instrumentation::MemoryInstrumentation::
              RequestGlobalDumpAndAppendToTrace,
          base::Unretained(
              memory_instrumentation::MemoryInstrumentation::GetInstance()),
          dump_type, level_of_detail, MemoryDumpDeterminism::kNone,
          std::move(callback)));
    } else {
      memory_instrumentation::MemoryInstrumentation::GetInstance()
          ->RequestGlobalDumpAndAppendToTrace(dump_type, level_of_detail,
                                              MemoryDumpDeterminism::kNone,
                                              std::move(callback));
    }
  }

 protected:
  void SetUp() override {
    next_request_index_ = 0;

    mock_dump_provider_ = std::make_unique<MockDumpProvider>();
    MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        mock_dump_provider_.get(), "TestDumpProvider", nullptr);
    MemoryDumpManager::GetInstance()
        ->set_dumper_registrations_ignored_for_testing(false);
    ContentBrowserTest::SetUp();
  }

  void TearDown() override {
    MemoryDumpManager::GetInstance()->UnregisterAndDeleteDumpProviderSoon(
        std::move(mock_dump_provider_));
    mock_dump_provider_.reset();
    ContentBrowserTest::TearDown();
  }

  void EnableMemoryTracing() {
    // Re-enabling tracing could crash these tests https://crbug.com/657628 .
    if (base::TrackEvent::IsEnabled()) {
      FAIL() << "Tracing seems to be already enabled. "
                "Very likely this is because the startup tracing file "
                "has been leaked from a previous test.";
    }
    // Enable tracing without periodic dumps.
    base::trace_event::TraceConfig trace_config(
        base::trace_event::TraceConfigMemoryTestUtil::
            GetTraceConfig_EmptyTriggers());

    base::RunLoop run_loop;
    bool success = TracingController::GetInstance()->StartTracing(
      trace_config, run_loop.QuitClosure());
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void DisableTracing() {
    base::RunLoop run_loop;
    bool success = TracingController::GetInstance()->StopTracing(
        TracingControllerImpl::CreateCallbackEndpoint(base::BindOnce(
            [](base::OnceClosure quit_closure,
               std::unique_ptr<std::string> trace_str) {
              std::move(quit_closure).Run();
            },
            run_loop.QuitClosure())));
    EXPECT_TRUE(success);
    run_loop.Run();
  }

  void RequestGlobalDumpAndWait(
      bool from_renderer_thread,
      const MemoryDumpType& dump_type,
      const MemoryDumpLevelOfDetail& level_of_detail) {
    base::RunLoop run_loop;
    RequestGlobalDumpWithClosure(from_renderer_thread, dump_type,
                                 level_of_detail, run_loop.QuitClosure());
    run_loop.Run();
  }

  void RequestGlobalDump(bool from_renderer_thread,
                         const MemoryDumpType& dump_type,
                         const MemoryDumpLevelOfDetail& level_of_detail) {
    RequestGlobalDumpWithClosure(from_renderer_thread, dump_type,
                                 level_of_detail, base::NullCallback());
  }

  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }

  MOCK_METHOD2(OnMemoryDumpDone,
               void(uint32_t request_index, RequestOutcome outcome));

  std::unique_ptr<MockDumpProvider> mock_dump_provider_;
  uint32_t next_request_index_;
  bool last_callback_success_;
};

// Flaky on Mac. crbug.com/809809
// TODO(crbug.com/40720107): OSMetrics::GetProcessMemoryMaps is not
// implemented on Fuchsia
#if BUILDFLAG(IS_MAC)
#define MAYBE_BrowserInitiatedDump DISABLED_BrowserInitiatedDump
#else
#define MAYBE_BrowserInitiatedDump BrowserInitiatedDump
#endif
// Checks that a memory dump initiated from a the main browser thread ends up in
// a successful dump.
IN_PROC_BROWSER_TEST_F(MemoryTracingTest, MAYBE_BrowserInitiatedDump) {
  Navigate(shell());

  EXPECT_CALL(*mock_dump_provider_, OnMemoryDump(_,_)).WillOnce(Return(true));
#if BUILDFLAG(IS_LINUX)
  // TODO(ssid): Test for dump success once the on start tracing done callback
  // is fixed to be called after enable tracing is acked by all processes,
  // crbug.com/709524. The test still tests if dumping does not crash.
  EXPECT_CALL(*this, OnMemoryDumpDone(_, _));
#else
  EXPECT_CALL(*this, OnMemoryDumpDone(_, RequestOutcome::kSuccess));
#endif

  EnableMemoryTracing();
  RequestGlobalDumpAndWait(false /* from_renderer_thread */,
                           MemoryDumpType::kExplicitlyTriggered,
                           MemoryDumpLevelOfDetail::kDetailed);
  DisableTracing();
}

}  // namespace content
