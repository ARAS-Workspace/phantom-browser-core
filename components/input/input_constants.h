// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INPUT_INPUT_CONSTANTS_H_
#define COMPONENTS_INPUT_INPUT_CONSTANTS_H_

#include "base/time/time.h"

namespace input {

// It would be nice to lower the desktop delay, but going any further with the
// modal dialog UI would be disruptive, and while new gentle UI indicating that
// a page is hung would be great, that UI isn't going to happen any time soon.
inline constexpr base::TimeDelta kHungRendererDelay = base::Seconds(15);

// The time to wait for a ping response from the main thread before declaring
// the renderer unresponsive.
inline constexpr base::TimeDelta kHungRendererPingTimeout = base::Seconds(1);

}  // namespace input

#endif  // COMPONENTS_INPUT_INPUT_CONSTANTS_H_
