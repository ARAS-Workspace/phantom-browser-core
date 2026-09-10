// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer/hi_res_timer_manager.h"

#include <memory>
#include <utility>

#include "base/base_switches.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_monitor_device_source.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

}  // namespace base
