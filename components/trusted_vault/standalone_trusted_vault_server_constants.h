// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_
#define COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_

#include <cstdint>
#include <vector>

#include "components/trusted_vault/trusted_vault_server_constants.h"

namespace trusted_vault {

std::vector<uint8_t> GetConstantTrustedVaultKey();

}  // namespace trusted_vault

#endif  // COMPONENTS_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_SERVER_CONSTANTS_H_
