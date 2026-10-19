// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/themes/ntp_custom_background_service_base.h"

class NtpCustomBackgroundService;
class PrefRegistrySimple;
class PrefService;
class Profile;

// Manages custom backgrounds on the new tab page.
class NtpCustomBackgroundService : public NtpCustomBackgroundServiceBase {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
  static void ResetNtpTheme(Profile* profile);
  static void ResetProfilePrefs(Profile* profile);

  explicit NtpCustomBackgroundService(Profile* profile);
  ~NtpCustomBackgroundService() override;

  // NtpCustomBackgroundServiceBase:
  void UpdateBackgroundFromSync() override;
  void ResetCustomBackgroundInfo() override;
  std::optional<CustomBackground> GetCustomBackground() override;

  // Returns whether having a custom background is disabled by policy.
  virtual bool IsCustomBackgroundDisabledByPolicy();

  // Returns whether a custom background has been set by the user.
  bool IsCustomBackgroundSet();

 private:
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_H_
