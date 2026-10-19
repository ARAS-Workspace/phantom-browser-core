// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
#define COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/themes/ntp_background_data.h"
#include "components/themes/ntp_custom_background_service_observer.h"
#include "url/gurl.h"

class PrefService;

// Shared base class for managing custom backgrounds on the NTP.
// Encapsulates platform-agnostic preference management.
class NtpCustomBackgroundServiceBase : public KeyedService {
 public:
  NtpCustomBackgroundServiceBase(
      PrefService* pref_service,
      const std::string& custom_background_dict_pref_name,
      const std::string& local_to_device_pref_name);
  ~NtpCustomBackgroundServiceBase() override;

  // Adds/Removes NtpCustomBackgroundServiceObserver observers.
  virtual void AddObserver(NtpCustomBackgroundServiceObserver* observer);
  virtual void RemoveObserver(NtpCustomBackgroundServiceObserver* observer);

  // Invoked when the background is reset on the NTP.
  // Virtual for testing.
  virtual void ResetCustomBackgroundInfo();

  // Invoked when a background pref update is received via sync, triggering
  // an update of theme info.
  // Virtual for testing.
  virtual void UpdateBackgroundFromSync();

  // Virtual for testing.
  virtual std::optional<CustomBackground> GetCustomBackground();

 protected:
  static base::DictValue NtpCustomBackgroundDefaults();
  // Returns false if the custom background pref cannot be parsed, otherwise
  // returns true.
  bool IsCustomBackgroundPrefValid();

  virtual void NotifyAboutBackgrounds();

  raw_ptr<PrefService, DanglingUntriaged> pref_service_;

 private:
  PrefChangeRegistrar pref_change_registrar_;
  const std::string custom_background_dict_pref_name_;
  const std::string local_to_device_pref_name_;

  base::ObserverList<NtpCustomBackgroundServiceObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NtpCustomBackgroundServiceBase> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_THEMES_NTP_CUSTOM_BACKGROUND_SERVICE_BASE_H_
