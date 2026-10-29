// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_switches.h"

namespace password_manager {

#if BUILDFLAG(IS_LINUX)
// Specifies which encryption storage backend to use. Possible values are
// kwallet, kwallet5, kwallet6, gnome-libsecret, basic.
// Any other value will lead to Chrome detecting the best backend automatically.
const char kPasswordStore[] = "password-store";
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace password_manager
