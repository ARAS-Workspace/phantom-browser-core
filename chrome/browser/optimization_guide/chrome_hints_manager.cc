// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/chrome_hints_manager.h"

#include "base/memory/scoped_refptr.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service.h"
#include "chrome/browser/navigation_predictor/navigation_predictor_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_web_contents_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/google/core/common/google_util.h"
#include "components/optimization_guide/core/hints/hint_cache.h"
#include "components/optimization_guide/core/hints/push_notification_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Returns true if we can make a request for hints for |prediction|.
}  // namespace

namespace optimization_guide {

ChromeHintsManager::ChromeHintsManager(
    Profile* profile,
    PrefService* pref_service,
    base::WeakPtr<optimization_guide::OptimizationGuideStore> hint_store,
    optimization_guide::TopHostProvider* top_host_provider,
    optimization_guide::TabUrlProvider* tab_url_provider,
    std::unique_ptr<optimization_guide::PushNotificationManager>
        push_notification_manager,
    signin::IdentityManager* identity_manager,
    OptimizationGuideLogger* optimization_guide_logger)
    : HintsManager(profile->IsOffTheRecord(),
                   g_browser_process->GetApplicationLocale(),
                   pref_service,
                   hint_store,
                   top_host_provider,
                   tab_url_provider,
                   std::move(push_notification_manager),
                   identity_manager,
                   optimization_guide_logger),
      profile_(profile) {
  if (!optimization_guide::features::IsSRPFetchingEnabled()) {
    return;
  }
  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile);
  if (navigation_predictor_service)
    navigation_predictor_service->AddObserver(this);
}

ChromeHintsManager::~ChromeHintsManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ChromeHintsManager::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  HintsManager::Shutdown();

  NavigationPredictorKeyedService* navigation_predictor_service =
      NavigationPredictorKeyedServiceFactory::GetForProfile(profile_);
  if (navigation_predictor_service)
    navigation_predictor_service->RemoveObserver(this);
}

void ChromeHintsManager::OnPredictionUpdated(
    const NavigationPredictorKeyedService::Prediction& prediction) {
}
}  // namespace optimization_guide
