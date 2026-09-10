// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bluetooth/chrome_bluetooth_chooser_controller.h"

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/chooser_controller/title_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

}  // namespace

ChromeBluetoothChooserController::ChromeBluetoothChooserController(
    content::RenderFrameHost* owner,
    const content::BluetoothChooser::EventHandler& event_handler)
    : permissions::BluetoothChooserController(
          owner,
          event_handler,
          CreateChooserTitle(owner, IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT)) {}

ChromeBluetoothChooserController::~ChromeBluetoothChooserController() = default;

void ChromeBluetoothChooserController::OpenAdapterOffHelpUrl() const {
}

void ChromeBluetoothChooserController::OpenPermissionPreferences() const {
#if BUILDFLAG(IS_MAC)
  base::mac::OpenSystemSettingsPane(
      base::mac::SystemSettingsPane::kPrivacySecurity_Bluetooth);
#else
  NOTREACHED();
#endif
}

void ChromeBluetoothChooserController::OpenHelpCenterUrl() const {}
