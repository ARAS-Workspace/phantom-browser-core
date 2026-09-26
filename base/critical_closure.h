// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CRITICAL_CLOSURE_H_
#define BASE_CRITICAL_CLOSURE_H_

#include <string_view>
#include <utility>

#include "base/functional/callback.h"
#include "base/location.h"
#include "build/ios_buildflags.h"

namespace base {

namespace internal {}  // namespace internal

// Returns a closure that will continue to run for a period of time when the
// application goes to the background if possible on platforms where
// applications don't execute while backgrounded, otherwise the original task is
// returned. If |is_immediate| is true, the closure will immediately prevent
// background suspension. Otherwise, the closure will wait to request background
// permission until it is run.
//
// Example:
//   file_task_runner_->PostTask(
//       FROM_HERE,
//       MakeCriticalClosure(task_name,
//                           base::BindOnce(&WriteToDiskTask, path_, data)));
//
// Note new closures might be posted in this closure. If the new closures need
// background running time, |MakeCriticalClosure| should be applied on them
// before posting. |task_name| is used by the platform to identify any tasks
// that do not complete in time for suspension.
//
// This function is used automatically for tasks posted to a sequence runner
// using TaskShutdownBehavior::BLOCK_SHUTDOWN.

inline OnceClosure MakeCriticalClosure(std::string_view task_name,
                                       OnceClosure closure,
                                       bool is_immediate) {
  // No-op for platforms where the application does not need to acquire
  // background time for closures to finish when it goes into the background.
  return closure;
}

inline OnceClosure MakeCriticalClosure(const Location& posted_from,
                                       OnceClosure closure,
                                       bool is_immediate) {
  return closure;
}

}  // namespace base

#endif  // BASE_CRITICAL_CLOSURE_H_
