// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nfc/chrome_nfc_permission_context_delegate.h"

#include "build/build_config.h"

ChromeNfcPermissionContextDelegate::ChromeNfcPermissionContextDelegate(
    std::unique_ptr<InteractabilityChecker> interactability_checker)
    : interactability_checker_(std::move(interactability_checker)) {}

ChromeNfcPermissionContextDelegate::~ChromeNfcPermissionContextDelegate() =
    default;
