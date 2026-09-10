// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/interactive_test_utils_aura.h"

#include "build/build_config.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "ui/aura/window.h"

namespace ui_test_utils {

void HideNativeWindow(gfx::NativeWindow window) {
  HideNativeWindowAura(window);
}

bool ShowAndFocusNativeWindow(gfx::NativeWindow window) {
  return ShowAndFocusNativeWindowAura(window);
}

void HideNativeWindowAura(gfx::NativeWindow window) {
  window->Hide();
}

bool ShowAndFocusNativeWindowAura(gfx::NativeWindow window) {
  window->Show();
  window->Focus();
  return true;
}

}  // namespace ui_test_utils
