// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"

// static
base::DictValue NtpCustomBackgroundServiceBase::NtpCustomBackgroundDefaults() {
  base::DictValue defaults;
  defaults.Set(kNtpCustomBackgroundURL, base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine1,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine2,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionActionURL,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundCollectionId,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundResumeToken,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundRefreshTimestamp,
               base::Value(base::Value::Type::INTEGER));
  return defaults;
}

NtpCustomBackgroundServiceBase::NtpCustomBackgroundServiceBase(
    PrefService* pref_service,
    const std::string& custom_background_dict_pref_name,
    const std::string& local_to_device_pref_name)
    : pref_service_(pref_service),
      custom_background_dict_pref_name_(custom_background_dict_pref_name),
      local_to_device_pref_name_(local_to_device_pref_name) {
  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      custom_background_dict_pref_name_,
      base::BindRepeating(
          &NtpCustomBackgroundServiceBase::UpdateBackgroundFromSync,
          weak_ptr_factory_.GetWeakPtr()));
}

NtpCustomBackgroundServiceBase::~NtpCustomBackgroundServiceBase() {
  for (auto& observer : observers_) {
    observer.OnNtpCustomBackgroundServiceShuttingDown();
  }
}

void NtpCustomBackgroundServiceBase::UpdateBackgroundFromSync() {
  pref_service_->SetBoolean(local_to_device_pref_name_, false);
  NotifyAboutBackgrounds();
}

std::optional<CustomBackground>
NtpCustomBackgroundServiceBase::GetCustomBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->GetBoolean(local_to_device_pref_name_)) {
    // This should be handled inside each platform.
    return std::nullopt;
  }

  // Attempt to get custom background URL from preferences.
  if (IsCustomBackgroundPrefValid()) {
    auto custom_background = std::make_optional<CustomBackground>();
    const base::DictValue& background_info =
        pref_service_->GetDict(custom_background_dict_pref_name_);
    const base::Value* color =
        background_info.Find(kNtpCustomBackgroundMainColor);
    if (color) {
      custom_background->custom_background_main_color =
          static_cast<uint32_t>(color->GetInt());
    }
    return custom_background;
  }

  return std::nullopt;
}

void NtpCustomBackgroundServiceBase::AddObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}
void NtpCustomBackgroundServiceBase::RemoveObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}
void NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_) {
    observer.OnCustomBackgroundImageUpdated();
  }
}

void NtpCustomBackgroundServiceBase::ResetCustomBackgroundInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool need_forced_refresh =
      pref_service_->GetBoolean(local_to_device_pref_name_) &&
      pref_service_->FindPreference(custom_background_dict_pref_name_)
          ->IsDefaultValue();

  pref_service_->SetBoolean(local_to_device_pref_name_, false);
  pref_service_->ClearPref(custom_background_dict_pref_name_);

  // If this device was using a local image and did not have a non-local
  // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
  // need to force a refresh here.
  if (need_forced_refresh) {
    NotifyAboutBackgrounds();
  }
}

bool NtpCustomBackgroundServiceBase::IsCustomBackgroundPrefValid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::DictValue& background_info =
      pref_service_->GetDict(custom_background_dict_pref_name_);

  const base::Value* background_url =
      background_info.Find(kNtpCustomBackgroundURL);
  if (!background_url) {
    return false;
  }

  return GURL(background_url->GetString()).is_valid();
}
