// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dm_token_utils.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

namespace policy {

namespace {

DMToken* GetTestingDMTokenStorage() {
  static base::NoDestructor<DMToken> dm_token(DMToken::CreateEmptyToken());
  return dm_token.get();
}

}  // namespace

DMToken GetDMToken(Profile* const profile) {
  DMToken dm_token = *GetTestingDMTokenStorage();
  return dm_token;
}

void SetDMTokenForTesting(const DMToken& dm_token) {
  *GetTestingDMTokenStorage() = dm_token;
}

}  // namespace policy
