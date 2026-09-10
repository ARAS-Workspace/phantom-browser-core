// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/default_browser/test_support/fake_shell_delegate.h"

#include <string>
#include <utility>

#include "base/functional/callback.h"

namespace default_browser {

FakeShellDelegate::FakeShellDelegate() = default;

FakeShellDelegate::~FakeShellDelegate() = default;

void FakeShellDelegate::StartCheckIsDefault(
    shell_integration::DefaultWebClientWorkerCallback callback) {
  std::move(callback).Run(default_state_);
}

}  // namespace default_browser
