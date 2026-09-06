// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/strings/escape.h"

namespace variations {

std::string EscapeValue(const std::string& value) {
  // This needs to be the inverse of UnescapeValue in
  // base/metrics/field_trial_params.
  std::string net_escaped_str =
      base::EscapeQueryParamValue(value, true /* use_plus */);

  // net doesn't escape '.' and '*' but base::UnescapeValue() covers those
  // cases.
  std::string escaped_str;
  escaped_str.reserve(net_escaped_str.length());
  for (const char ch : net_escaped_str) {
    if (ch == '.') {
      escaped_str.append("%2E");
    } else if (ch == '*') {
      escaped_str.append("%2A");
    } else {
      escaped_str.push_back(ch);
    }
  }
  return escaped_str;
}

bool AssociateParamsFromString(const std::string& varations_string) {
  return base::AssociateFieldTrialParamsFromString(varations_string,
                                                   &base::UnescapeValue);
}

}  // namespace variations
