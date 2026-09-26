// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process_platform_part_base.h"

#include "base/notreached.h"
#include "build/build_config.h"

#include "chrome/browser/lifetime/application_lifetime_desktop.h"

BrowserProcessPlatformPartBase::BrowserProcessPlatformPartBase() = default;

BrowserProcessPlatformPartBase::~BrowserProcessPlatformPartBase() = default;

void BrowserProcessPlatformPartBase::StartTearDown() {
}

void BrowserProcessPlatformPartBase::AttemptExit(bool try_to_quit_application) {
  // On most platforms, closing all windows causes the application to exit.
  chrome::CloseAllBrowsers();
}

void BrowserProcessPlatformPartBase::PreMainMessageLoopRun() {}

void BrowserProcessPlatformPartBase::PostDestroyThreads() {}
