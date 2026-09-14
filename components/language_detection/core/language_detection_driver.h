// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_DRIVER_H_
#define COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_DRIVER_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"

namespace language_detection {

struct LanguageDetectionDetails;

// Interface for an object that reports the detected language of a page to
// observers. A concrete implementation must be provided by the embedder.
class COMPONENT_EXPORT(LANGUAGE_DETECTION) LanguageDetectionDriver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when the observed instance is being destroyed so that observers
    // can reset their pointers to the LanguageDetectionDriver.
    virtual void OnLanguageDetectionDriverDestroyed(
        LanguageDetectionDriver* driver) {}

    // Called when the page language has been detected.
    virtual void OnLanguageDetermined(const LanguageDetectionDetails& details) {
    }
  };

  LanguageDetectionDriver();
  LanguageDetectionDriver(LanguageDetectionDriver&&) = delete;
  LanguageDetectionDriver& operator=(LanguageDetectionDriver&&) = delete;
  LanguageDetectionDriver(const LanguageDetectionDriver&) = delete;
  LanguageDetectionDriver& operator=(const LanguageDetectionDriver&) = delete;
  virtual ~LanguageDetectionDriver();

  // Adds or removes observers.
  void AddLanguageDetectionObserver(Observer* observer);
  void RemoveLanguageDetectionObserver(Observer* observer);

 protected:
  const base::ObserverList<Observer, true>& language_detection_observers()
      const {
    return language_detection_observers_;
  }

 private:
  base::ObserverList<Observer, true> language_detection_observers_;
};

}  // namespace language_detection

namespace base {

template <>
struct ScopedObservationTraits<
    language_detection::LanguageDetectionDriver,
    language_detection::LanguageDetectionDriver::Observer> {
  static void AddObserver(
      language_detection::LanguageDetectionDriver* source,
      language_detection::LanguageDetectionDriver::Observer* observer) {
    source->AddLanguageDetectionObserver(observer);
  }
  static void RemoveObserver(
      language_detection::LanguageDetectionDriver* source,
      language_detection::LanguageDetectionDriver::Observer* observer) {
    source->RemoveLanguageDetectionObserver(observer);
  }
};

}  // namespace base

#endif  // COMPONENTS_LANGUAGE_DETECTION_CORE_LANGUAGE_DETECTION_DRIVER_H_
