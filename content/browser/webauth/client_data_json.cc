// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/client_data_json.h"

namespace content {

std::string BuildClientDataJson(webauthn::ClientDataJsonParams params) {
  return webauthn::BuildClientDataJson(std::move(params), std::nullopt);
}

}  // namespace content
