// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/metrics/chrome_browser_sampling_trials.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_recorder_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
