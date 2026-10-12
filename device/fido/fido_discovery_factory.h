// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "services/device/public/mojom/usb_manager.mojom.h"

#if BUILDFLAG(IS_MAC)
#include "base/apple/owned_objc.h"
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace device {

// FidoDiscoveryFactory offers methods to construct instances of
// FidoDiscoveryBase for a given |transport| protocol.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryFactory {
 public:
  FidoDiscoveryFactory();
  virtual ~FidoDiscoveryFactory();

  // Instantiates one or more FidoDiscoveryBases for the given transport.
  //
  // FidoTransportProtocol::kUsbHumanInterfaceDevice is not valid on Android.
  virtual std::vector<std::unique_ptr<FidoDiscoveryBase>> Create(
      FidoTransportProtocol transport);

  // Returns whether the current instance is an override injected by the
  // WebAuthn testing API.
  virtual bool IsTestOverride();

  void set_hid_ignore_list(base::flat_set<VidPid> hid_ignore_list);

#if BUILDFLAG(IS_MAC)
  // Configures the Touch ID authenticator. Set to std::nullopt to disable it.
  void set_mac_touch_id_info(
      std::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config) {
    mac_touch_id_config_ = std::move(mac_touch_id_config);
  }
  // Sets the window on top of which macOS will show any iCloud Keychain UI.
  void set_nswindow(base::apple::WeakNSWindow window) { nswindow_ = window; }
  // Sets a flag to allow for discovery of the authenticator despite having no
  // NSWindow. For testing use only.
  void set_allow_no_nswindow_for_testing(bool value) {
    allow_no_nswindow_for_testing_ = value;
  }
#endif  // BUILDFLAG(IS_MAC)

 protected:
  static std::vector<std::unique_ptr<FidoDiscoveryBase>> SingleDiscovery(
      std::unique_ptr<FidoDiscoveryBase> discovery);

 private:
#if BUILDFLAG(IS_MAC)
  std::vector<std::unique_ptr<FidoDiscoveryBase>> MaybeCreatePlatformDiscovery()
      const;
#endif

#if BUILDFLAG(IS_MAC)
  std::optional<fido::mac::AuthenticatorConfig> mac_touch_id_config_;
  base::apple::WeakNSWindow nswindow_;
  bool allow_no_nswindow_for_testing_ = false;
#endif  // BUILDFLAG(IS_MAC)
  base::flat_set<VidPid> hid_ignore_list_;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_FACTORY_H_
