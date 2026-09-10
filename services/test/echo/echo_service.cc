// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/echo_service.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/check_is_test.h"
#include "base/check.h"
#include "base/debug/stack_trace.h"
#include "base/immediate_crash.h"
#include "base/memory/shared_memory_mapping.h"
#include "build/build_config.h"

#include <string>

#include "base/memory/scoped_refptr.h"
#include "components/os_crypt/async/common/encryptor.h"

namespace echo {

EchoService::EchoService(mojo::PendingReceiver<mojom::EchoService> receiver)
    : receiver_(this, std::move(receiver)) {}

EchoService::~EchoService() = default;

void EchoService::EchoString(const std::string& input,
                             EchoStringCallback callback) {
  std::move(callback).Run(input);
}

void EchoService::EchoStringToSharedMemory(
    const std::string& input,
    base::UnsafeSharedMemoryRegion region) {
  base::WritableSharedMemoryMapping mapping = region.Map();
  base::span(mapping).copy_prefix_from(base::as_byte_span(input));
}

void EchoService::Quit() {
  receiver_.reset();
}

void EchoService::Crash() {
  base::ImmediateCrash();
}

void EchoService::DecryptEncrypt(
    scoped_refptr<os_crypt_async::Encryptor> encryptor,
    const std::vector<uint8_t>& input,
    DecryptEncryptCallback callback) {
  CHECK(encryptor->IsDecryptionAvailable());
  // Take the input, which was encrypted in the caller process, and decrypt it.
  const auto plaintext = encryptor->DecryptData(input);
  if (!plaintext.has_value()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  CHECK(encryptor->IsEncryptionAvailable());
  // Encrypt it again using the key inside this process, and return the
  // encrypted ciphertext to the caller.
  std::move(callback).Run(encryptor->EncryptString(*plaintext));
}

void EchoService::VerifyCheckIsTest(VerifyCheckIsTestCallback callback) {
  CHECK_IS_TEST();
  std::move(callback).Run(true);
}

}  // namespace echo
