// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_factory.h"

#include <algorithm>

#include "base/notreached.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/hid/fido_hid_discovery.h"
#include "device/fido/public/features.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/discovery.h"
#include "device/fido/mac/icloud_keychain.h"
#endif  // BUILDFLAG(IS_MAC)

namespace device {

FidoDiscoveryFactory::FidoDiscoveryFactory() = default;
FidoDiscoveryFactory::~FidoDiscoveryFactory() = default;

std::vector<std::unique_ptr<FidoDiscoveryBase>> FidoDiscoveryFactory::Create(
    FidoTransportProtocol transport) {
  switch (transport) {
    case FidoTransportProtocol::kUsbHumanInterfaceDevice:
      return SingleDiscovery(
          std::make_unique<FidoHidDiscovery>(hid_ignore_list_));
    case FidoTransportProtocol::kBluetoothLowEnergy:
      return {};
    case FidoTransportProtocol::kHybrid:
      return {};
    case FidoTransportProtocol::kNearFieldCommunication:
      // TODO(crbug.com/40568770): Add NFC support.
      return {};
    case FidoTransportProtocol::kInternal: {
      std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries;
#if BUILDFLAG(IS_MAC)
      discoveries = MaybeCreatePlatformDiscovery();
#endif
      return discoveries;
    }
    case FidoTransportProtocol::kSmartCard:
      return {};
    case FidoTransportProtocol::kDeprecatedAoa:
      NOTREACHED() << "Android Open Accessory is deprecated.";
  }
  NOTREACHED() << "Unhandled transport type";
}

bool FidoDiscoveryFactory::IsTestOverride() {
  return false;
}

void FidoDiscoveryFactory::set_hid_ignore_list(
    base::flat_set<VidPid> hid_ignore_list) {
  hid_ignore_list_ = std::move(hid_ignore_list);
}

// static
std::vector<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::SingleDiscovery(
    std::unique_ptr<FidoDiscoveryBase> discovery) {
  if (!discovery) {
    return {};
  }

  std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
  ret.emplace_back(std::move(discovery));
  return ret;
}

#if BUILDFLAG(IS_MAC)
std::vector<std::unique_ptr<FidoDiscoveryBase>>
FidoDiscoveryFactory::MaybeCreatePlatformDiscovery() const {
  std::vector<std::unique_ptr<FidoDiscoveryBase>> ret;
  if (mac_touch_id_config_) {
    ret.emplace_back(std::make_unique<fido::mac::FidoTouchIdDiscovery>(
        *mac_touch_id_config_));
  }
  if (fido::icloud_keychain::IsSupported() &&
      (nswindow_ || allow_no_nswindow_for_testing_)) {
    ret.emplace_back(fido::icloud_keychain::NewDiscovery(nswindow_));
  }
  return ret;
}
#endif

}  // namespace device
