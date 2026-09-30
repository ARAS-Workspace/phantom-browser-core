// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_otp_phish_guard_delegate.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"

namespace autofill {
ChromeOtpPhishGuardDelegate::ChromeOtpPhishGuardDelegate(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)) {}

ChromeOtpPhishGuardDelegate::~ChromeOtpPhishGuardDelegate() = default;

void ChromeOtpPhishGuardDelegate::StartOtpPhishGuardCheck(
    const GURL& main_frame_url,
    const GURL& frame_to_fill_url,
    base::OnceCallback<void(bool)> callback) {
  auto* password_manager_client =
      ChromePasswordManagerClient::FromWebContents(&web_contents_.get());
  bool is_actor_task_ongoing =
      password_manager_client && password_manager_client->IsActorTaskActive();

  // If Safe Browsing is disabled/unavailable and an actor task is ongoing,
  // do not consider the check successful (report unsafe / is_malicious = true).
  // Post task to ensure callback is always invoked asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), is_actor_task_ongoing));
}

}  // namespace autofill
