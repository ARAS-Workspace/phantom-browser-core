// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STARTUP_DATA_H_
#define CHROME_BROWSER_STARTUP_DATA_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace policy {
class ProfilePolicyConnector;
class SchemaRegistryService;
class UserCloudPolicyManager;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

class PrefService;
class ProfileKey;
class ChromeFeatureListCreator;

// The StartupData owns any pre-created objects in //chrome before the full
// browser starts, including the ChromeFeatureListCreator and the Profile's
// PrefService. See doc:
// https://docs.google.com/document/d/1ybmGWRWXu0aYNxA99IcHFesDAslIaO1KFP6eGdHTJaE/edit#heading=h.7bk05syrcom
class StartupData {
 public:
  StartupData();

  StartupData(const StartupData&) = delete;
  StartupData& operator=(const StartupData&) = delete;

  ~StartupData();

  // Records core profile settings into the SystemProfileProto. It is important
  // when Chrome is running in the reduced mode, which doesn't start UMA
  // recording but persists all of the UMA data into a memory mapped file. The
  // file will be picked up by Chrome next time it is launched in the full
  // browser mode.
  void RecordCoreSystemProfile();

  // TODO(martinkong): Remove this function and replace its usage with
  // ChromeFeatureListCreator::GetInstance()
  ChromeFeatureListCreator* chrome_feature_list_creator();

 private:
};

#endif  // CHROME_BROWSER_STARTUP_DATA_H_
