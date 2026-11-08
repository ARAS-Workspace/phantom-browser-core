// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include "base/values.h"

namespace policy {

PolicyStatusProvider::PolicyStatusProvider() = default;

PolicyStatusProvider::~PolicyStatusProvider() = default;

void PolicyStatusProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PolicyStatusProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::DictValue PolicyStatusProvider::GetStatus() {
  // This method is called when the client is not enrolled.
  // Thus return an empty dictionary.
  return base::DictValue();
}

void PolicyStatusProvider::NotifyStatusChange() {
  for (auto& observer : observers_) {
    observer.OnPolicyStatusChanged();
  }
}

}  // namespace policy
