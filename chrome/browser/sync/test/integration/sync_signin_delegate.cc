// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_signin_delegate.h"

#include <memory>

#include "base/check.h"
#include "build/build_config.h"

#include "chrome/browser/sync/test/integration/fake_sync_signin_delegate_desktop.h"
#include "chrome/browser/sync/test/integration/live_sync_signin_delegate_desktop.h"

std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegateWithFakeSignin(
    Profile* profile) {
  CHECK(profile);
  return std::make_unique<FakeSyncSigninDelegateDesktop>(profile);
}

std::unique_ptr<SyncSigninDelegate> CreateSyncSigninDelegateWithLiveSignin(
    Profile* profile) {
  CHECK(profile);
  return std::make_unique<LiveSyncSigninDelegateDesktop>(profile);
}
