// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_status_icon.h"

#include "chrome/browser/device_notifications/device_status_icon_renderer.h"

UsbStatusIcon::UsbStatusIcon()
    : UsbSystemTrayIcon(std::make_unique<DeviceStatusIconRenderer>(this)) {}

UsbStatusIcon::~UsbStatusIcon() = default;
