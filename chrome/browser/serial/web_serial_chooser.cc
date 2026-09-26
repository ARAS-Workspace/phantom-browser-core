// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/serial/web_serial_chooser.h"

#include "chrome/browser/ui/serial/serial_chooser_controller.h"

#include "chrome/browser/serial/web_serial_chooser_desktop.h"

std::unique_ptr<WebSerialChooser> WebSerialChooser::Create(
    content::RenderFrameHost* frame,
    std::unique_ptr<SerialChooserController> controller) {
  std::unique_ptr<WebSerialChooser> chooser;
  chooser = std::make_unique<WebSerialChooserDesktop>();
  chooser->ShowChooser(frame, std::move(controller));
  return chooser;
}

WebSerialChooser::WebSerialChooser() = default;
