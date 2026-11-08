// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
#define COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/values.h"
#include "components/policy/policy_export.h"

namespace policy {

// An interface for querying the status of a policy provider.  It surfaces
// things like last fetch time or status of the backing store, but not the
// actual policies themselves.
class POLICY_EXPORT PolicyStatusProvider {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPolicyStatusChanged() = 0;
  };

  PolicyStatusProvider();
  PolicyStatusProvider(const PolicyStatusProvider&) = delete;
  PolicyStatusProvider& operator=(const PolicyStatusProvider&) = delete;
  virtual ~PolicyStatusProvider();

  // Returns a dictionary with metadata about policies.
  virtual base::DictValue GetStatus();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyStatusChange();

 private:
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_WEBUI_POLICY_STATUS_PROVIDER_H_
