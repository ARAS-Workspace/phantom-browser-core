// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/persisted_data.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/event_history.h"
#include "chrome/updater/registration_data.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client_errors.h"

namespace {

// PersistedData keys.
constexpr char kVersionPath[] = "pv_path";
constexpr char kVersionKey[] = "pv_key";
constexpr char kECP[] = "ecp";
constexpr char kBC[] = "bc";
constexpr char kBP[] = "bp";
constexpr char kAP[] = "ap";
constexpr char kAPPath[] = "ap_path";
constexpr char kAPKey[] = "ap_key";
constexpr char kLang[] = "lang";

constexpr char kHadApps[] = "had_apps";
constexpr char kRemoteLoggingCookie[] = "remote_logging_cookie";
constexpr char kNextAllowedLoggingAttemptTime[] = "next_logging_attempt_time";
constexpr char kEulaRequired[] = "eula_required";

constexpr char kLastChecked[] = "last_checked";
constexpr char kLastStarted[] = "last_started";
constexpr char kLastOSVersion[] = "last_os_version";

constexpr char kCookieValueKey[] = "value";
constexpr char kCookieExpirationKey[] = "expiration";

}  // namespace

namespace updater {

PersistedData::PersistedData(
    UpdaterScope scope,
    PrefService* pref_service,
    std::unique_ptr<update_client::ActivityDataService> activity_service)
    : scope_(scope),
      pref_service_(pref_service),
      delegate_(update_client::CreatePersistedData(
          base::BindRepeating(
              [](PrefService* pref_service) { return pref_service; },
              pref_service),
          std::move(activity_service))) {
  CHECK(pref_service_);

  PersistedDataEvent event;
  for (const std::string& app_id : GetAppIds()) {
    PersistedDataEvent::RegisteredApp app;
    app.app_id = app_id;
    app.brand_code = GetBrandCode(app_id);
    app.cohort = GetCohort(app_id);
    app.version = GetProductVersion(app_id).GetString();
    event.AddRegisteredApp(app);
  }
  event.SetEulaRequired(GetEulaRequired())
      .SetLastChecked(GetLastChecked())
      .SetLastStarted(GetLastStarted())
      .WriteAsync();
}

PersistedData::~PersistedData() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::Version PersistedData::GetProductVersion(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetProductVersion(id);
}

void PersistedData::SetProductVersion(const std::string& id,
                                      const base::Version& pv) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(pv.IsValid());
  delegate_->SetProductVersion(id, pv);

}

base::Version PersistedData::GetMaxPreviousProductVersion(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetMaxPreviousProductVersion(id);
}

void PersistedData::SetMaxPreviousProductVersion(
    const std::string& id,
    const base::Version& max_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(max_version.IsValid());
  delegate_->SetMaxPreviousProductVersion(id, max_version);
}

base::FilePath PersistedData::GetProductVersionPath(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kVersionPath));
}

void PersistedData::SetProductVersionPath(const std::string& id,
                                          const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kVersionPath, path.AsUTF8Unsafe());
}

std::string PersistedData::GetProductVersionKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kVersionKey);
}

void PersistedData::SetProductVersionKey(const std::string& id,
                                         const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kVersionKey, key);
}

std::string PersistedData::GetFingerprint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetFingerprint(id);
}

void PersistedData::SetFingerprint(const std::string& id,
                                   const std::string& fingerprint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetFingerprint(id, fingerprint);
}

base::FilePath PersistedData::GetExistenceCheckerPath(
    const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kECP));
}

void PersistedData::SetExistenceCheckerPath(const std::string& id,
                                            const base::FilePath& ecp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kECP, ecp.AsUTF8Unsafe());
}

std::string PersistedData::GetBrandCode(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string bc = GetString(id, kBC);

  if (bc.empty()) {
    return {};
  }

  return bc;
}

void PersistedData::SetBrandCode(const std::string& id, const std::string& bc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If there is already an existing brand code, do not overwrite it.
  if (!GetBrandCode(id).empty()) {
    return;
  }

  SetString(id, kBC, bc);

}

base::FilePath PersistedData::GetBrandPath(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kBP));
}

void PersistedData::SetBrandPath(const std::string& id,
                                 const base::FilePath& bp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kBP, bp.AsUTF8Unsafe());
}

std::string PersistedData::GetAP(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return GetString(id, kAP);
}

void PersistedData::SetAP(const std::string& id, const std::string& ap) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAP, ap);

}

base::FilePath PersistedData::GetAPPath(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::FilePath::FromUTF8Unsafe(GetString(id, kAPPath));
}

void PersistedData::SetAPPath(const std::string& id,
                              const base::FilePath& path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAPPath, path.AsUTF8Unsafe());
}

std::string PersistedData::GetAPKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetString(id, kAPKey);
}

void PersistedData::SetAPKey(const std::string& id, const std::string& key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetString(id, kAPKey, key);
}

std::string PersistedData::GetLang(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const std::string lang = GetString(id, kLang);

  return lang;
}

void PersistedData::SetLang(const std::string& id, const std::string& lang) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SetString(id, kLang, lang);

}

int PersistedData::GetDateLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDateLastActive(id);
}

int PersistedData::GetDaysSinceLastActive(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDaysSinceLastActive(id);
}

void PersistedData::SetDateLastActive(const std::string& id, int dla) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastActive(id, dla);
}

int PersistedData::GetDateLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDateLastRollCall(id);
}

int PersistedData::GetDaysSinceLastRollCall(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetDaysSinceLastRollCall(id);
}

void PersistedData::SetDateLastRollCall(const std::string& id, int dlrc) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastRollCall(id, dlrc);
}

std::string PersistedData::GetCohort(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohort(id);
}

void PersistedData::SetCohort(const std::string& id,
                              const std::string& cohort) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohort(id, cohort);

}

std::string PersistedData::GetCohortName(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohortName(id);
}

void PersistedData::SetCohortName(const std::string& id,
                                  const std::string& cohort_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohortName(id, cohort_name);

}

std::string PersistedData::GetCohortHint(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetCohortHint(id);
}

void PersistedData::SetCohortHint(const std::string& id,
                                  const std::string& cohort_hint) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetCohortHint(id, cohort_hint);
}

std::string PersistedData::GetPingFreshness(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetPingFreshness(id);
}

void PersistedData::SetDateLastData(const std::vector<std::string>& ids,
                                    int datenum,
                                    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetDateLastData(ids, datenum, std::move(callback));
}

int PersistedData::GetInstallDate(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetInstallDate(id);
}

void PersistedData::SetInstallDate(const std::string& id, int install_date) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetInstallDate(id, install_date);
}

std::string PersistedData::GetInstallId(const std::string& app_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetInstallId(app_id);
}

void PersistedData::SetInstallId(const std::string& app_id,
                                 const std::string& install_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetInstallId(app_id, install_id);
}

void PersistedData::GetActiveBits(
    const std::vector<std::string>& ids,
    base::OnceCallback<void(const std::set<std::string>&)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->GetActiveBits(ids, std::move(callback));
}

base::Time PersistedData::GetThrottleUpdatesUntil() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delegate_->GetThrottleUpdatesUntil();
}

void PersistedData::SetLastUpdateCheckError(
    const update_client::CategorizedError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetLastUpdateCheckError(error);
}

void PersistedData::SetThrottleUpdatesUntil(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->SetThrottleUpdatesUntil(time);
}

void PersistedData::RegisterApp(const RegistrationRequest& rq) {
  VLOG(2) << __func__ << ": Registering " << rq.app_id;
  if (base::Version(rq.version).IsValid()) {
    VLOG(2) << __func__ << ": app version " << rq.version;
    SetProductVersion(rq.app_id, base::Version(rq.version));
  }
  if (rq.version_path && !rq.version_path->empty()) {
    SetProductVersionPath(rq.app_id, *rq.version_path);
  }
  if (rq.version_key && !rq.version_key->empty()) {
    SetProductVersionKey(rq.app_id, *rq.version_key);
  }
  if (!rq.existence_checker_path.empty()) {
    SetExistenceCheckerPath(rq.app_id, rq.existence_checker_path);
  }
  if (rq.lang && !rq.lang->empty()) {
    SetLang(rq.app_id, *rq.lang);
  }
  if (!rq.brand_code.empty()) {
    SetBrandCode(rq.app_id, rq.brand_code);
  }
  if (!rq.brand_path.empty()) {
    SetBrandPath(rq.app_id, rq.brand_path);
  }
  if (!rq.ap.empty()) {
    SetAP(rq.app_id, rq.ap);
  }
  if (rq.ap_path && !rq.ap_path->empty()) {
    SetAPPath(rq.app_id, *rq.ap_path);
  }
  if (rq.ap_key && !rq.ap_key->empty()) {
    SetAPKey(rq.app_id, *rq.ap_key);
  }
  if (rq.dla) {
    SetDateLastActive(rq.app_id, rq.dla.value());
  } else if (GetDateLastActive(rq.app_id) == update_client::kDateUnknown) {
    SetDateLastActive(rq.app_id, update_client::kDateFirstTime);
  }
  if (rq.dlrc) {
    SetDateLastRollCall(rq.app_id, rq.dlrc.value());
  } else if (GetDateLastRollCall(rq.app_id) == update_client::kDateUnknown) {
    SetDateLastRollCall(rq.app_id, update_client::kDateFirstTime);
  }
  if (rq.install_date) {
    SetInstallDate(rq.app_id, *rq.install_date);
  }
  if (rq.cohort && !rq.cohort->empty()) {
    SetCohort(rq.app_id, *rq.cohort);
  }
  if (rq.cohort_name && !rq.cohort_name->empty()) {
    SetCohortName(rq.app_id, *rq.cohort_name);
  }
  if (rq.cohort_hint && !rq.cohort_hint->empty()) {
    SetCohortHint(rq.app_id, *rq.cohort_hint);
  }
  if (rq.install_id && !rq.install_id->empty()) {
    SetInstallId(rq.app_id, *rq.install_id);
  }
}

bool PersistedData::HasApp(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::DictValue* apps =
      pref_service_->GetDict(update_client::kPersistedDataPreference)
          .FindDict("apps");
  return apps && apps->Find(base::ToLowerASCII(id)) != nullptr;
}

bool PersistedData::RemoveApp(const std::string& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!pref_service_) {
    return false;
  }

  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  base::DictValue* apps = update->FindDict("apps");

  return apps ? apps->Remove(base::ToLowerASCII(id)) : false;
}

std::vector<std::string> PersistedData::GetAppIds() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The prefs is a dictionary of dictionaries, where each inner dictionary
  // corresponds to an app:
  // {"updateclientdata":{"apps":{"{44FC7FE2-65CE-487C-93F4-EDEE46EEAAAB}":{...
  const base::DictValue& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::DictValue* apps = dict.FindDict("apps");
  if (!apps) {
    return {};
  }
  std::vector<std::string> app_ids;
  for (auto it = apps->begin(); it != apps->end(); ++it) {
    const auto& app_id = it->first;
    const auto pv = GetProductVersion(app_id);
    if (pv.IsValid()) {
      app_ids.push_back(app_id);
    }
  }
  return app_ids;
}

const base::DictValue* PersistedData::GetAppKey(const std::string& id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return nullptr;
  }
  const base::DictValue& dict =
      pref_service_->GetDict(update_client::kPersistedDataPreference);
  const base::DictValue* apps = dict.FindDict("apps");
  if (!apps) {
    return nullptr;
  }
  return apps->FindDict(base::ToLowerASCII(id));
}

std::string PersistedData::GetString(const std::string& id,
                                     const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::DictValue* app_key = GetAppKey(id);
  if (!app_key) {
    return {};
  }
  const std::string* value = app_key->FindString(key);
  if (!value) {
    return {};
  }
  return *value;
}

base::DictValue* PersistedData::GetOrCreateAppKey(const std::string& id,
                                                  base::DictValue& root) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue* apps = root.EnsureDict("apps");
  base::DictValue* app = apps->EnsureDict(base::ToLowerASCII(id));
  return app;
}

std::optional<int> PersistedData::GetInteger(const std::string& id,
                                             const std::string& key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return std::nullopt;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  base::DictValue* apps = update->FindDict("apps");
  if (!apps) {
    return std::nullopt;
  }
  base::DictValue* app = apps->FindDict(base::ToLowerASCII(id));
  if (!app) {
    return std::nullopt;
  }
  return app->FindInt(key);
}

void PersistedData::SetInteger(const std::string& id,
                               const std::string& key,
                               int value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->Set(key, value);
}

void PersistedData::SetString(const std::string& id,
                              const std::string& key,
                              const std::string& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return;
  }
  ScopedDictPrefUpdate update(pref_service_,
                              update_client::kPersistedDataPreference);
  GetOrCreateAppKey(id, update.Get())->Set(key, value);
}

bool PersistedData::GetHadApps() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ && pref_service_->GetBoolean(kHadApps);
}

void PersistedData::SetHadApps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetBoolean(kHadApps, true);
  }
}

std::optional<PersistedData::Cookie> PersistedData::GetRemoteLoggingCookie()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return std::nullopt;
  }

  const base::DictValue& cookie = pref_service_->GetDict(kRemoteLoggingCookie);
  const std::string* value = cookie.FindString(kCookieValueKey);
  std::optional<base::Time> expiration =
      base::ValueToTime(cookie.Find(kCookieExpirationKey));
  if (!value || !expiration) {
    return std::nullopt;
  }

  return Cookie{
      .value = *value,
      .expiration = *expiration,
  };
}

void PersistedData::SetRemoteLoggingCookie(const Cookie& logging_cookie) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetDict(
        kRemoteLoggingCookie,
        base::DictValue()
            .Set(kCookieValueKey, logging_cookie.value)
            .Set(kCookieExpirationKey,
                 base::TimeToValue(logging_cookie.expiration)));
  }
}

void PersistedData::ClearRemoteLoggingCookie() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->ClearPref(kRemoteLoggingCookie);
  }
}

base::Time PersistedData::GetNextAllowedLoggingAttemptTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_service_) {
    return base::Time();
  }
  return pref_service_->GetTime(kNextAllowedLoggingAttemptTime);
}

void PersistedData::SetNextAllowedLoggingAttemptTime(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetTime(kNextAllowedLoggingAttemptTime, time);
  }
}

bool PersistedData::GetEulaRequired() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_ && pref_service_->GetBoolean(kEulaRequired);
}

void PersistedData::SetEulaRequired(bool eula_required) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetBoolean(kEulaRequired, eula_required);
  }
}

base::Time PersistedData::GetLastChecked() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastChecked);
}

void PersistedData::SetLastChecked(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetTime(kLastChecked, time);
  }
}

base::Time PersistedData::GetLastStarted() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return pref_service_->GetTime(kLastStarted);
}

void PersistedData::SetLastStarted(base::Time time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pref_service_) {
    pref_service_->SetTime(kLastStarted, time);
  }
}

// Register persisted data prefs, except for kPersistedDataPreference.
// kPersistedDataPreference is registered by update_client::RegisterPrefs.
void RegisterPersistedDataPrefs(scoped_refptr<PrefRegistrySimple> registry) {
  registry->RegisterBooleanPref(kHadApps, false);
  registry->RegisterBooleanPref(kEulaRequired, false);
  registry->RegisterTimePref(kNextAllowedLoggingAttemptTime, {});
  registry->RegisterTimePref(kLastChecked, {});
  registry->RegisterTimePref(kLastStarted, {});
  registry->RegisterStringPref(kLastOSVersion, {});
  registry->RegisterDictionaryPref(kRemoteLoggingCookie, {});
}

}  // namespace updater
