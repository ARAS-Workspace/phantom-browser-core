// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/web_usb_chooser.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/usb/usb_chooser_controller.h"

#include "chrome/browser/usb/web_usb_chooser_desktop.h"

// static
std::unique_ptr<WebUsbChooser> WebUsbChooser::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<UsbChooserController> controller) {
  std::unique_ptr<WebUsbChooser> chooser;
  chooser = std::make_unique<WebUsbChooserDesktop>();
  chooser->ShowChooser(render_frame_host, std::move(controller));
  return chooser;
}

WebUsbChooser::~WebUsbChooser() = default;

WebUsbChooser::WebUsbChooser() = default;
