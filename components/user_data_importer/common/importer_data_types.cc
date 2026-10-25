// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_data_importer/common/importer_data_types.h"

#include "build/build_config.h"

namespace user_data_importer {

SourceProfile::SourceProfile()
    : importer_type(TYPE_UNKNOWN), services_supported(0) {}

SourceProfile::SourceProfile(const SourceProfile& other) = default;

SourceProfile::~SourceProfile() = default;

}  // namespace user_data_importer
