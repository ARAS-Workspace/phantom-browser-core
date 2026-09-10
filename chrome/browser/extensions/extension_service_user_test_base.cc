// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_service_user_test_base.h"

#include <memory>
#include <utility>

#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

ExtensionServiceUserTestBase::ExtensionServiceUserTestBase() = default;
ExtensionServiceUserTestBase::~ExtensionServiceUserTestBase() = default;
ExtensionServiceUserTestBase::ExtensionServiceUserTestBase(
    std::unique_ptr<content::BrowserTaskEnvironment> task_environment)
    : ExtensionServiceTestBase(std::move(task_environment)) {}

void ExtensionServiceUserTestBase::MaybeSetUpTestUser(bool is_guest) {
  SetGuestSessionOnProfile(is_guest);

  ASSERT_EQ(is_guest, profile()->IsGuestSession());

}

}  // namespace extensions
