// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FIELD_TRIAL_CONFIG_FIELD_TRIAL_UTIL_H_
#define COMPONENTS_VARIATIONS_FIELD_TRIAL_CONFIG_FIELD_TRIAL_UTIL_H_

#include <string>

namespace variations {

// Unescapes special characters from the given string.
std::string UnescapeValue(const std::string& value);

// Escapes the trial name, or parameter name, or parameter value in a way that
// makes it usable within variations::switches::kForceFieldTrialParams.
std::string EscapeValue(const std::string& value);

// Provides a mechanism to associate multiple set of params to multiple groups
// with a formatted string specified from commandline. See
// kForceFieldTrialParams in components/variations/variations_switches.cc for
// more details on the formatting.
bool AssociateParamsFromString(const std::string& variations_string);

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FIELD_TRIAL_CONFIG_FIELD_TRIAL_UTIL_H_
