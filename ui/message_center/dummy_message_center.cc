// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/message_center/message_center.h"

#error This file should only be used in Android and Fuchsia.

namespace message_center {

// static
void MessageCenter::Initialize() {
}

// static
MessageCenter* MessageCenter::Get() {
  return NULL;
}

// static
void MessageCenter::Shutdown() {
}

MessageCenter::MessageCenter() {
}

MessageCenter::~MessageCenter() {
}

}  // namespace message_center
