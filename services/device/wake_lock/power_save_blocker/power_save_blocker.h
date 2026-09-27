// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_WAKE_LOCK_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_
#define SERVICES_DEVICE_WAKE_LOCK_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_

#include <memory>
#include <string>

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "build/build_config.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace device {

// A RAII-style class to block the system from entering low-power (sleep) mode.
// This class is thread-safe; it may be constructed and deleted on any thread.
class PowerSaveBlocker {
 public:
  // Pass in the type of power save blocking desired. If multiple types of
  // blocking are desired, instantiate one PowerSaveBlocker for each type.
  // |reason| and |description| (a more-verbose, human-readable justification of
  // the blocking) may be provided to the underlying system APIs on some
  // platforms.
  PowerSaveBlocker(mojom::WakeLockType type,
                   mojom::WakeLockReason reason,
                   const std::string& description,
                   scoped_refptr<base::SequencedTaskRunner> ui_task_runner);

  PowerSaveBlocker(const PowerSaveBlocker&) = delete;
  PowerSaveBlocker& operator=(const PowerSaveBlocker&) = delete;

  virtual ~PowerSaveBlocker();

 private:
  class Delegate;

  base::SequenceBound<Delegate> delegate_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_WAKE_LOCK_POWER_SAVE_BLOCKER_POWER_SAVE_BLOCKER_H_
