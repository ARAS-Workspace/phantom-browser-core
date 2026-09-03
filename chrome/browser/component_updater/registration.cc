// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/registration.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/app_provisioning_component_installer.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/common/chrome_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/history_search_strings_component_installer.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/real_time_url_checks_allowlist_component_installer.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#include "chrome/browser/component_updater/smart_dim_component_installer.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

namespace component_updater {

namespace {

// Runs in the thread pool, may block.
void DeleteOldComponents(const base::FilePath& user_data_dir) {
  for (const base::FilePath::StringType& dir : {
           FILE_PATH_LITERAL("DesktopSharingHub"),    // Remove in M146+
           FILE_PATH_LITERAL("CookieReadinessList"),  // Remove in M146+
           FILE_PATH_LITERAL("OpenCookieDatabase"),   // Remove in M146+
           FILE_PATH_LITERAL("TpcdMetadata"),         // Remove in M147+
           FILE_PATH_LITERAL(
               "ProbabilisticRevealTokenRegistry"),  // Remove in M148+
           FILE_PATH_LITERAL("AutofillStates"),      // Remove in M153+
           FILE_PATH_LITERAL(
               "Fingerprinting Protection Filter"),    // Remove in M156+
           FILE_PATH_LITERAL("PlusAddressBlocklist"),  // Remove in M158+
#if BUILDFLAG(IS_CHROMEOS)
           // TODO(crbug.com/380780352): Remove these after the stepping stone.
           FILE_PATH_LITERAL("lacros-dogfood-canary"),
           FILE_PATH_LITERAL("lacros-dogfood-dev"),
           FILE_PATH_LITERAL("lacros-dogfood-beta"),
           FILE_PATH_LITERAL("lacros-dogfood-stable"),
#endif  // BUILDFLAG(IS_CHROMEOS)
       }) {
    base::DeletePathRecursively(user_data_dir.Append(dir));
  }
}

}  // namespace

void RegisterComponentsForUpdate() {
  auto* const cus = g_browser_process->component_updater();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  RegisterRecoveryImprovedComponent(cus, g_browser_process->local_state());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  RegisterWidevineCdmComponent(cus);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if !BUILDFLAG(IS_CHROMEOS)
  // CRLSetFetcher attempts to load a CRL set from either the local disk or
  // network.
  // For Chrome OS this registration is delayed until user login.
  component_updater::RegisterCRLSetComponent(cus);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  MaybeRegisterPKIMetadataComponent(cus);

#if BUILDFLAG(IS_CHROMEOS)
  RegisterSmartDimComponent(cus);
  RegisterAppProvisioningComponent(cus);
  apps::chrome_app_deprecation::RegisterAllowlistComponentUpdater(cus);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
  RegisterRealTimeUrlChecksAllowlistComponent(cus);
#endif  // BUIDLFLAG(IS_ANDROID)

  base::FilePath path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &path)) {
    if (!history_embeddings::IsHistoryEmbeddingsFeatureEnabled()) {
      DeleteHistorySearchStringsComponent(path);
    }
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&DeleteOldComponents, path));
  }
}

}  // namespace component_updater
