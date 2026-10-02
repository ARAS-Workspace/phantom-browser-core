// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_BLOCKLIST_H_
#define EXTENSIONS_BROWSER_BLOCKLIST_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

// The blocklist of extensions backed by safe browsing.
class Blocklist : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Observes `blocklist` on construction and unobserves on destruction.
    explicit Observer(Blocklist* blocklist);

    virtual void OnBlocklistUpdated() = 0;

   protected:
    ~Observer() override;

   private:
    raw_ptr<Blocklist> blocklist_;
  };

  using BlocklistStateMap = std::map<ExtensionId, BlocklistState>;

  using GetBlocklistedIDsCallback =
      base::OnceCallback<void(const BlocklistStateMap&)>;

  using IsBlocklistedCallback = base::OnceCallback<void(BlocklistState)>;

  explicit Blocklist(content::BrowserContext* context);

  Blocklist(const Blocklist&) = delete;
  Blocklist& operator=(const Blocklist&) = delete;

  ~Blocklist() override;

  // From the set of extension IDs passed in via `ids`, asynchronously checks
  // which are blocklisted and includes them in the resulting map passed
  // via `callback`, which will be sent on the caller's message loop. The values
  // of the map are the blocklist state for each extension. Extensions with
  // a BlocklistState of NOT_BLOCKLISTED are not included in the result.
  //
  // For a synchronous version which ONLY CHECKS CURRENTLY INSTALLED EXTENSIONS
  // see ExtensionPrefs::IsExtensionBlocklisted.
  void GetBlocklistedIDs(const std::set<ExtensionId>& ids,
                         GetBlocklistedIDsCallback callback);

  // More convenient form of GetBlocklistedIDs for checking a single extension.
  void IsBlocklisted(const ExtensionId& extension_id,
                     IsBlocklistedCallback callback);

  // Adds/removes an observer to the blocklist.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  raw_ptr<content::BrowserContext> context_;

  base::ObserverList<Observer> observers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_BLOCKLIST_H_
