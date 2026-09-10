// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/crash/crash_reporting_breakpad.h"

namespace remoting {

// Not implemented for Mac, see https://crbug.com/714714
void InitializeBreakpadReporting() {
  // Touch the object to make sure it is initialized.
}

}  // namespace remoting
