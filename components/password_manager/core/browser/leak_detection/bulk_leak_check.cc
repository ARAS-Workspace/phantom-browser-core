// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"

#include <utility>

namespace password_manager {

LeakCheckCredential::LeakCheckCredential(std::u16string username,
                                         std::u16string password)
    : username_(std::move(username)), password_(std::move(password)) {}

LeakCheckCredential::LeakCheckCredential(LeakCheckCredential&&) = default;

LeakCheckCredential& LeakCheckCredential::operator=(LeakCheckCredential&&) =
    default;

LeakCheckCredential::~LeakCheckCredential() = default;

}  // namespace password_manager
