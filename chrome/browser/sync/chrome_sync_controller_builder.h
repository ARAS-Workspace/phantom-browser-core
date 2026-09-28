// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_
#define CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/prefs/pref_service.h"
#include "components/themes/cross_device/cross_device_theme_tracker.h"
#include "extensions/buildflags/buildflags.h"

class Profile;
class SecurityEventRecorder;


namespace syncer {
class DataTypeController;
class DataTypeStoreService;
class SyncService;
}  // namespace syncer

namespace sync_pb {
class ThemeSpecifics;
class ThemeAndroidSpecifics;
}  // namespace sync_pb

namespace webapk {
class WebApkSyncService;
}  // namespace webapk

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
class ExtensionSyncService;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ThemeService;

namespace web_app {
class WebAppProvider;
}  // namespace web_app
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)




// Class responsible for instantiating sync controllers (DataTypeController)
// for datatypes / features under chrome/.
//
// NOTE: prefer adding new types to browser_sync::CommonControllerBuilder if the
// type is available in components/, even if it's not enabled on all embedders
// or platforms.
//
// Users of this class need to inject dependencies by invoking all setters (more
// on this below) and finally invoke `Build()` to instantiate controllers.
class ChromeSyncControllerBuilder {
 public:
  ChromeSyncControllerBuilder();
  ~ChromeSyncControllerBuilder();

  using LocalThemeSpecifics = themes::LocalThemeSpecifics;

  // Setters to inject dependencies. Each of these setters must be invoked
  // before invoking `Build()`. In some cases it is allowed to inject nullptr.
  void SetCrossDeviceThemeTracker(
      themes::CrossDeviceThemeTracker<LocalThemeSpecifics>*
          cross_device_theme_tracker);
  void SetDataTypeStoreService(
      syncer::DataTypeStoreService* data_type_store_service);
  void SetSecurityEventRecorder(SecurityEventRecorder* security_event_recorder);

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  void SetExtensionSyncService(ExtensionSyncService* extension_sync_service);
  void SetExtensionSystemProfile(Profile* profile);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void SetThemeService(ThemeService* theme_service);
  void SetWebAppProvider(web_app::WebAppProvider* web_app_provider);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)




  // Actually builds the controllers. All setters above must have been called
  // beforehand (null may or may not be allowed).
  std::vector<std::unique_ptr<syncer::DataTypeController>> Build(
      syncer::SyncService* sync_service);

 private:
  // Minimalistic fork of std::optional that enforces via CHECK that it has a
  // value when accessing it.
  template <typename Ptr>
  class SafeOptional {
   public:
    SafeOptional() = default;
    ~SafeOptional() = default;

    void Set(Ptr ptr) {
      CHECK(!ptr_.has_value());
      ptr_.emplace(std::move(ptr));
    }

    // Set() must have been called before.
    Ptr value() const {
      CHECK(ptr_.has_value());
      return ptr_.value();
    }

   private:
    std::optional<Ptr> ptr_;
  };

  // For all above, nullopt indicates the corresponding setter wasn't invoked.
  // nullptr indicates the setter was invoked with nullptr.
  SafeOptional<raw_ptr<themes::CrossDeviceThemeTracker<LocalThemeSpecifics>>>
      cross_device_theme_tracker_;
  SafeOptional<raw_ptr<syncer::DataTypeStoreService>> data_type_store_service_;
  SafeOptional<raw_ptr<SecurityEventRecorder>> security_event_recorder_;

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  SafeOptional<raw_ptr<ExtensionSyncService>> extension_sync_service_;
  // This Profile instance has nothing special and is just the profile being
  // exercised by the factory. A more tailored name is used simply to limit its
  // usage beyond extensions.
  SafeOptional<raw_ptr<Profile>> extension_system_profile_;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  SafeOptional<raw_ptr<ThemeService>> theme_service_;
  SafeOptional<raw_ptr<web_app::WebAppProvider>> web_app_provider_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)


};

#endif  // CHROME_BROWSER_SYNC_CHROME_SYNC_CONTROLLER_BUILDER_H_
