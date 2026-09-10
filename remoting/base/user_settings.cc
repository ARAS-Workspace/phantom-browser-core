// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/user_settings.h"

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/build_config.h"

namespace remoting {

UserSettings::UserSettings() = default;

UserSettings::~UserSettings() = default;

UserSettings* UserSettings::GetInstance() {
  NOTREACHED();
}

}  // namespace remoting
