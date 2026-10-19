// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_custom_background_service.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace {

void RemoveLocalBackgroundImageCopy(Profile* profile) {
  // Delete uploaded image.
  base::FilePath path = profile->GetPath().AppendASCII(
      chrome::kChromeUIUntrustedNewTabPageBackgroundFilename);
  base::ThreadPool::PostTask(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::GetDeleteFileCallback(path));
}

}  // namespace

// static
void NtpCustomBackgroundService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(
      prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse,
      NtpCustomBackgroundDefaults(),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);

  registry->RegisterDictionaryPref(prefs::kNtpCustomBackgroundDict,
                                   NtpCustomBackgroundDefaults());
  registry->RegisterBooleanPref(prefs::kNtpCustomBackgroundLocalToDevice,
                                false);
  registry->RegisterStringPref(prefs::kNtpCustomBackgroundLocalToDeviceId, "");
}

// static
void NtpCustomBackgroundService::ResetNtpTheme(Profile* profile) {
  auto* pref_service = profile->GetPrefs();
  RemoveLocalBackgroundImageCopy(profile);
  pref_service->ClearPref(prefs::kNtpCustomBackgroundDict);
  pref_service->SetBoolean(prefs::kNtpCustomBackgroundLocalToDevice, false);
  pref_service->ClearPref(prefs::kNtpCustomBackgroundLocalToDeviceId);
}

// static
void NtpCustomBackgroundService::ResetProfilePrefs(Profile* profile) {
  // Clear theme.
  ResetNtpTheme(profile);
}

NtpCustomBackgroundService::NtpCustomBackgroundService(Profile* profile)
    : NtpCustomBackgroundServiceBase(profile->GetPrefs(),
                                     prefs::kNtpCustomBackgroundDict,
                                     prefs::kNtpCustomBackgroundLocalToDevice),
      profile_(profile) {}

NtpCustomBackgroundService::~NtpCustomBackgroundService() = default;

void NtpCustomBackgroundService::UpdateBackgroundFromSync() {
  // Any incoming change to synced background data should clear the local image.
  RemoveLocalBackgroundImageCopy(profile_);
  pref_service_->ClearPref(prefs::kNtpCustomBackgroundLocalToDeviceId);
  NtpCustomBackgroundServiceBase::UpdateBackgroundFromSync();
}

void NtpCustomBackgroundService::ResetCustomBackgroundInfo() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (IsCustomBackgroundDisabledByPolicy()) {
    return;
  }

  RemoveLocalBackgroundImageCopy(profile_);
  pref_service_->ClearPref(prefs::kNtpCustomBackgroundLocalToDeviceId);

  NtpCustomBackgroundServiceBase::ResetCustomBackgroundInfo();
}

std::optional<CustomBackground>
NtpCustomBackgroundService::GetCustomBackground() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    // A local image counts as a custom background without a main color.
    return std::make_optional<CustomBackground>();
  }

  return NtpCustomBackgroundServiceBase::GetCustomBackground();
}

bool NtpCustomBackgroundService::IsCustomBackgroundDisabledByPolicy() {
  // |prefs::kNtpCustomBackgroundDict| is managed by policy only if
  // |policy::key::kNTPCustomBackgroundEnabled| is set to false and therefore
  // should be empty.
  bool managed =
      pref_service_->IsManagedPreference(prefs::kNtpCustomBackgroundDict);
  if (managed) {
    DCHECK(pref_service_->GetDict(prefs::kNtpCustomBackgroundDict).empty());
  }
  return managed;
}

bool NtpCustomBackgroundService::IsCustomBackgroundSet() {
  return pref_service_->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice) ||
         IsCustomBackgroundPrefValid();
}
