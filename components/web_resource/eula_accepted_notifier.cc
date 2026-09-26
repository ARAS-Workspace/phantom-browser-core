// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_resource/eula_accepted_notifier.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"

namespace web_resource {

EulaAcceptedNotifier::EulaAcceptedNotifier(PrefService* local_state)
    : local_state_(local_state), observer_(nullptr) {
}

EulaAcceptedNotifier::~EulaAcceptedNotifier() {
}

void EulaAcceptedNotifier::Init(Observer* observer) {
  DCHECK(!observer_ && observer);
  observer_ = observer;
}

bool EulaAcceptedNotifier::IsEulaAccepted() {
  if (local_state_->GetBoolean(prefs::kEulaAccepted))
    return true;

  // Register for the notification, if this is the first time.
  if (registrar_.IsEmpty()) {
    registrar_.Init(local_state_);
    registrar_.Add(prefs::kEulaAccepted,
                   base::BindRepeating(&EulaAcceptedNotifier::OnPrefChanged,
                                       base::Unretained(this)));
  }
  return false;
}

// static
EulaAcceptedNotifier* EulaAcceptedNotifier::Create(PrefService* local_state) {
  return nullptr;
}

void EulaAcceptedNotifier::NotifyObserver() {
  observer_->OnEulaAccepted();
}

void EulaAcceptedNotifier::OnPrefChanged() {
  DCHECK(!registrar_.IsEmpty());
  registrar_.RemoveAll();

  DCHECK(local_state_->GetBoolean(prefs::kEulaAccepted));
  observer_->OnEulaAccepted();
}

}  // namespace web_resource
