// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/enterprise_companion/lock.h"

#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/enterprise_companion/enterprise_companion_branding.h"
#include "components/named_system_lock/lock.h"

namespace {

#if BUILDFLAG(IS_LINUX)
constexpr char kLockName[] = "/" PRODUCT_FULLNAME_STRING ".lock";
#elif BUILDFLAG(IS_MAC)
constexpr char kLockName[] = MAC_BUNDLE_IDENTIFIER_STRING ".lock";
#endif

}  // namespace

namespace enterprise_companion {

std::unique_ptr<ScopedLock> CreateScopedLock(base::TimeDelta timeout) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return named_system_lock::ScopedLock::Create(kLockName, timeout);
#endif
}

}  // namespace enterprise_companion
