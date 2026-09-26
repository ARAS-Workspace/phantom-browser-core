// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>

#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/memory_instrumentation.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Le;
using testing::Ge;
using testing::AllOf;
using memory_instrumentation::GlobalMemoryDump;
using memory_instrumentation::mojom::ProcessType;

namespace content {

class MemoryInstrumentationTest : public ContentBrowserTest {
 protected:
  void Navigate(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(shell, GetTestUrl("", "title1.html")));
  }
};

uint64_t GetPrivateFootprintKb() {
  uint64_t private_footprint_kb = 0;
  base::RunLoop run_loop;
  memory_instrumentation::MemoryInstrumentation::GetInstance()
      ->RequestPrivateMemoryFootprint(
          base::GetCurrentProcId(),
          base::BindOnce(
              [](base::OnceClosure quit_closure, uint64_t* private_footprint_kb,
                 memory_instrumentation::mojom::RequestOutcome outcome,
                 std::unique_ptr<GlobalMemoryDump> result) {
                // The global dump should only contain the current process
                // (browser).
                EXPECT_EQ(
                    memory_instrumentation::mojom::RequestOutcome::kSuccess,
                    outcome);
                ASSERT_EQ(std::distance(result->process_dumps().begin(),
                                        result->process_dumps().end()),
                          1);
                const auto& process_dump = *result->process_dumps().begin();
                ASSERT_EQ(ProcessType::BROWSER, process_dump.process_type());

                *private_footprint_kb =
                    process_dump.os_dump().private_footprint_kb;
                std::move(quit_closure).Run();
              },
              run_loop.QuitClosure(), &private_footprint_kb));
  run_loop.Run();
  return private_footprint_kb;
}

// *SAN fake some sys calls we need meaning we never get dumps for the
// processes.
#if defined(MEMORY_SANITIZER) || defined(ADDRESS_SANITIZER) || \
    defined(THREAD_SANITIZER)
#define MAYBE_PrivateFootprintComputation DISABLED_PrivateFootprintComputation
#else
#define MAYBE_PrivateFootprintComputation PrivateFootprintComputation
#endif

// Despite the location, this test is not tracing related.
// TODO(hjd): Move this once we have a resource_coordinator folder in browser.
IN_PROC_BROWSER_TEST_F(MemoryInstrumentationTest,
                       MAYBE_PrivateFootprintComputation) {

  Navigate(shell());

  // We have to pick a big size (>=64mb) to avoid an implementation detail of
  // malloc on MacOS which doesn't free or mark as reusable small allocations
  // after a free.
  const int64_t kAllocSize = 65 * 1024 * 1024;
  const int64_t kAllocSizeKb = kAllocSize / 1024;

  int64_t before_kb = GetPrivateFootprintKb();

  std::vector<char> buffer(kAllocSize, 1);
  // Read from the buffer to ensure it's committed.
  volatile char first = buffer.front();
  volatile char last = buffer.back();
  EXPECT_EQ(first + last, 2);

  int64_t during_kb = GetPrivateFootprintKb();

  buffer.clear();
  buffer.shrink_to_fit();

  int64_t after_kb = GetPrivateFootprintKb();

  EXPECT_THAT(after_kb - before_kb,
              AllOf(Ge(-kAllocSizeKb / 10), Le(kAllocSizeKb / 10)));
  EXPECT_THAT(during_kb - before_kb,
              AllOf(Ge(kAllocSizeKb - 3000), Le(kAllocSizeKb + 3000)));
  EXPECT_THAT(during_kb - after_kb,
              AllOf(Ge(kAllocSizeKb - 3000), Le(kAllocSizeKb + 3000)));
}

}  // namespace content
