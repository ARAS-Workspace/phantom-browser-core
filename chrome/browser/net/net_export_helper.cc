// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/net_export_helper.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/browser/ui/webui/extensions/extension_basic_info.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#endif

namespace chrome_browser_net {

base::DictValue GetPrerenderInfo(Profile* profile) {
  prerender::NoStatePrefetchManager* no_state_prefetch_manager =
      prerender::NoStatePrefetchManagerFactory::GetForBrowserContext(profile);
  if (no_state_prefetch_manager) {
    return no_state_prefetch_manager->CopyAsDict();
  } else {
    base::DictValue dict;
    dict.Set("enabled", false);
    dict.Set("omnibox_enabled", false);
    return dict;
  }
}

base::ListValue GetExtensionInfo(Profile* profile) {
  base::ListValue extension_list;
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto* extension_registrar = extensions::ExtensionRegistrar::Get(profile);
  if (extension_registrar) {
    const extensions::ExtensionSet extensions =
        extensions::ExtensionRegistry::Get(profile)
            ->GenerateInstalledExtensionsSet();
    for (const auto& extension : extensions) {
      base::DictValue extension_info;
      bool enabled = extension_registrar->IsExtensionEnabled(extension->id());
      extensions::GetExtensionBasicInfo(extension.get(), enabled,
                                        &extension_info);
      extension_list.Append(std::move(extension_info));
    }
  }
#endif
  return extension_list;
}

#if BUILDFLAG(IS_ANDROID)
void PublishNetLogToDownloads(const base::FilePath& file_path) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          [](const base::FilePath& path) {
            if (!base::CopyFileToDownloadsCollection(path,
                                                     "application/json")) {
              DLOG(ERROR) << "Failed to copy net-export log to public "
                             "Downloads collection: "
                          << path.value();
            }
          },
          file_path));
}
#endif

}  // namespace chrome_browser_net
