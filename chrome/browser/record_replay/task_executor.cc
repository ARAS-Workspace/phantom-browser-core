// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/task_executor.h"

#include <algorithm>
#include <string>

#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/record_replay/core/browser/task_definition.pb.h"
#include "components/tabs/public/tab_interface.h"

namespace record_replay {

void TaskExecutor::ExecuteTask(
    Profile* profile,
    BrowserWindowInterface* browser_window,
    const TaskDefinition& definition,
    const std::vector<TaskParameter>& parameter_values) {}

}  // namespace record_replay
