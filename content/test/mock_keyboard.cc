// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_keyboard.h"

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"

namespace content {

MockKeyboard::MockKeyboard() {}

MockKeyboard::~MockKeyboard() {
}

int MockKeyboard::GetCharacters(Layout layout,
                                int key_code,
                                Modifiers modifiers,
                                std::u16string* output) {
  NOTIMPLEMENTED();
  return -1;
}

}  // namespace content
