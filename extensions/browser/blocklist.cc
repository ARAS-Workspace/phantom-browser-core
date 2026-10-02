// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/blocklist.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

namespace {

void CheckOneExtensionState(Blocklist::IsBlocklistedCallback callback,
                            const Blocklist::BlocklistStateMap& state_map) {
  std::move(callback).Run(state_map.empty() ? NOT_BLOCKLISTED
                                            : state_map.begin()->second);
}

}  // namespace

Blocklist::Observer::Observer(Blocklist* blocklist) : blocklist_(blocklist) {
  blocklist_->AddObserver(this);
}

Blocklist::Observer::~Observer() {
  blocklist_->RemoveObserver(this);
}

Blocklist::Blocklist(content::BrowserContext* context) : context_(context) {}

Blocklist::~Blocklist() = default;

void Blocklist::GetBlocklistedIDs(const std::set<ExtensionId>& ids,
                                  GetBlocklistedIDsCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), BlocklistStateMap()));
}

void Blocklist::IsBlocklisted(const ExtensionId& extension_id,
                              IsBlocklistedCallback callback) {
  std::set<ExtensionId> check;
  check.insert(extension_id);
  GetBlocklistedIDs(
      check, base::BindOnce(&CheckOneExtensionState, std::move(callback)));
}

void Blocklist::AddObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.AddObserver(observer);
}

void Blocklist::RemoveObserver(Observer* observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
