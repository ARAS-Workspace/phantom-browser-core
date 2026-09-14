// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/language_detection/core/language_detection_driver.h"

namespace language_detection {

LanguageDetectionDriver::LanguageDetectionDriver() = default;

LanguageDetectionDriver::~LanguageDetectionDriver() {
  for (auto& observer : language_detection_observers()) {
    observer.OnLanguageDetectionDriverDestroyed(this);
  }
}

void LanguageDetectionDriver::AddLanguageDetectionObserver(Observer* observer) {
  language_detection_observers_.AddObserver(observer);
}

void LanguageDetectionDriver::RemoveLanguageDetectionObserver(
    Observer* observer) {
  language_detection_observers_.RemoveObserver(observer);
}

}  // namespace language_detection
