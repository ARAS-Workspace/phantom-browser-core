// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/text_input_client.h"

#include <iomanip>
#include <ios>
#include <ostream>
#include <string_view>

namespace ui {

TextInputClient::~TextInputClient() {
}

bool TextInputClient::CanInsertImage() {
  return false;
}

#if BUILDFLAG(IS_MAC)
bool TextInputClient::SupportsAutoFill() const {
  return true;
}
#endif  // BUILDFLAG(IS_MAC)

}  // namespace ui
