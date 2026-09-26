// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/device_reauth/device_authenticator_common.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/device_reauth/mac/authenticator_mac.h"
#include "chrome/browser/device_reauth/mac/device_authenticator_mac.h"
#endif

using content::BrowserContext;
using device_reauth::DeviceAuthenticator;

ChromeDeviceAuthenticatorFactory::ChromeDeviceAuthenticatorFactory()
    : ProfileKeyedServiceFactory(
          "ChromeDeviceAuthenticator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {}

ChromeDeviceAuthenticatorFactory::~ChromeDeviceAuthenticatorFactory() = default;

// static
ChromeDeviceAuthenticatorFactory*
ChromeDeviceAuthenticatorFactory::GetInstance() {
  static base::NoDestructor<ChromeDeviceAuthenticatorFactory> instance;
  return instance.get();
}

// static
std::unique_ptr<DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetForProfile(
    Profile* profile,
    const gfx::NativeWindow window,
    const device_reauth::DeviceAuthParams& params) {
  return ChromeDeviceAuthenticatorFactory::GetForProfile(profile, params);
}

// static
std::unique_ptr<DeviceAuthenticator>
ChromeDeviceAuthenticatorFactory::GetForProfile(
    Profile* profile,
    const device_reauth::DeviceAuthParams& params) {
  DeviceAuthenticatorProxy* proxy = static_cast<DeviceAuthenticatorProxy*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));

  CHECK(proxy);

#if BUILDFLAG(IS_MAC)
  auto device_authenticator = std::make_unique<DeviceAuthenticatorMac>(
      std::make_unique<AuthenticatorMac>(), proxy, params);
#else
  static_assert(false);
#endif
  return std::move(device_authenticator);
}

std::unique_ptr<KeyedService>
ChromeDeviceAuthenticatorFactory::BuildServiceInstanceForBrowserContext(
    BrowserContext* context) const {

  return std::make_unique<DeviceAuthenticatorProxy>();
}
