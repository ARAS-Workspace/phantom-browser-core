// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key.h"

#include <limits>
#include <optional>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_rust.h"
#include "base/containers/span_writer.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/ecdsa_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/mock_unexportable_key.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "crypto/tpm_parser.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

using ::testing::ElementsAre;
using ::testing::Return;

// Small helper to create a seriailzed TPM2_Certify response. This allows us to
// use tpm.rs in the test expectations.
std::vector<uint8_t> ConstructFakeTpmResponse(
    base::span<const uint8_t> statement,
    base::span<const uint8_t> signature) {
  size_t size = 2 + 4 + 4 + 2 + statement.size() + signature.size();
  std::vector<uint8_t> resp(size);
  base::SpanWriter<uint8_t> writer(resp);
  writer.WriteU16BigEndian(0x8001);  // TPM_ST_NO_SESSIONS
  writer.WriteU32BigEndian(size);
  writer.WriteU32BigEndian(0);  // responseCode = TPM_RC_SUCCESS
  writer.WriteU16BigEndian(statement.size());
  writer.Write(statement);
  writer.Write(signature);
  CHECK_EQ(writer.remaining(), 0u);
  return resp;
}

enum class Provider {
  kTPM,
  kFake,
  kMicrosoftSoftware,
};

const Provider kAllProviders[] = {
    Provider::kTPM,
    Provider::kFake,
    Provider::kMicrosoftSoftware,
};

const crypto::SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
    crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
    crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
};

#if BUILDFLAG(IS_APPLE)
constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
#endif  // BUILDFLAG(IS_APPLE)

std::string ToString(Provider provider) {
  switch (provider) {
    case Provider::kTPM:
      return "TPM";
    case Provider::kFake:
      return "Fake";
    case Provider::kMicrosoftSoftware:
      return "Microsoft Software";
  }
}

class UnexportableKeyTest
    : public testing::TestWithParam<
          std::tuple<crypto::SignatureVerifier::SignatureAlgorithm, Provider>> {
 protected:
  std::unique_ptr<crypto::UnexportableKeyProvider> CreateProvider() {
    if (provider_type() == Provider::kMicrosoftSoftware) {
      return crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
    }

    crypto::UnexportableKeyProvider::Config config{
#if BUILDFLAG(IS_APPLE)
        .keychain_access_group = kTestKeychainAccessGroup
#endif  // BUILDFLAG(IS_APPLE)
    };
    return crypto::GetUnexportableKeyProvider(std::move(config));
  }

  crypto::SignatureVerifier::SignatureAlgorithm algorithm() {
    return std::get<0>(GetParam());
  }

  Provider provider_type() { return std::get<1>(GetParam()); }

  bool CurrentAlgorithmSupported(crypto::UnexportableKeyProvider* provider) {
    if (!provider) {
      return false;
    }
    const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
        algorithm()};
    return provider->SelectAlgorithm(algorithms) == algorithm();
  }

  crypto::sign::SignatureKind signature_kind() {
    switch (algorithm()) {
      case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        return crypto::sign::SignatureKind::ECDSA_SHA256;
      case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        return crypto::sign::SignatureKind::RSA_PKCS1_SHA256;
      case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
        return crypto::sign::SignatureKind::RSA_PKCS1_SHA1;
      case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
        return crypto::sign::SignatureKind::RSA_PSS_SHA256;
    }
  }

 private:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 scoped_fake_keychain_{
      kTestKeychainAccessGroup};
#endif  // BUILDFLAG(IS_MAC)
};

INSTANTIATE_TEST_SUITE_P(All,
                         UnexportableKeyTest,
                         testing::Combine(testing::ValuesIn(kAllAlgorithms),
                                          testing::ValuesIn(kAllProviders)));

TEST_P(UnexportableKeyTest, RoundTrip) {
  const bool expected_is_hardware_backed =
      provider_type() == Provider::kFake ? false
                                         : provider_type() == Provider::kTPM;

  switch (algorithm()) {
    case crypto::SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
      LOG(INFO) << "ECDSA P-256, provider=" << ToString(provider_type());
      break;
    case crypto::SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
      LOG(INFO) << "RSA, provider=" << ToString(provider_type());
      break;
    default:
      ASSERT_TRUE(false);
  }

  SCOPED_TRACE(static_cast<int>(algorithm()));
  SCOPED_TRACE(ToString(provider_type()));

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!CurrentAlgorithmSupported(provider.get())) {
    GTEST_SKIP() << "Algorithm not supported by provider.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};
  const base::TimeTicks generate_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key =
      provider->GenerateSigningKeySlowly(algorithms);
  if (provider_type() == Provider::kFake) {
    ASSERT_TRUE(key);
  } else if (!key) {
    GTEST_SKIP() << "Key generation failed (see https://crbug.com/41494935).";
  }

  EXPECT_EQ(key->IsHardwareBacked(), expected_is_hardware_backed);
  LOG(INFO) << "Generation took " << (base::TimeTicks::Now() - generate_start);

  ASSERT_EQ(key->Algorithm(), algorithm());
  const std::vector<uint8_t> wrapped = key->GetWrappedKey();
  const std::vector<uint8_t> spki = key->GetSubjectPublicKeyInfo();
  const uint8_t msg[] = {1, 2, 3, 4};

  const base::TimeTicks sign_start = base::TimeTicks::Now();
  const std::optional<std::vector<uint8_t>> sig = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign_start);
  ASSERT_TRUE(sig);

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(algorithm(), *sig, spki));
  verifier.VerifyUpdate(msg);
  ASSERT_TRUE(verifier.VerifyFinal());

  const base::TimeTicks import2_start = base::TimeTicks::Now();
  std::unique_ptr<crypto::UnexportableSigningKey> key2 =
      provider->FromWrappedSigningKeySlowly(wrapped);
  if (!key2) {
    GTEST_SKIP()
        << "Importing wrapped key failed (see https://crbug.com/41494935).";
  }
  LOG(INFO) << "Import took " << (base::TimeTicks::Now() - import2_start);

  const base::TimeTicks sign2_start = base::TimeTicks::Now();
  const std::optional<std::vector<uint8_t>> sig2 = key->SignSlowly(msg);
  LOG(INFO) << "Signing took " << (base::TimeTicks::Now() - sign2_start);
  ASSERT_TRUE(sig2);

  crypto::SignatureVerifier verifier2;
  ASSERT_TRUE(verifier2.VerifyInit(algorithm(), *sig2, spki));
  verifier2.VerifyUpdate(msg);
  ASSERT_TRUE(verifier2.VerifyFinal());

  crypto::StatefulUnexportableKeyProvider* stateful_provider =
      provider->AsStatefulUnexportableKeyProvider();
  EXPECT_TRUE(stateful_provider == nullptr ||
              stateful_provider->DeleteWrappedKeysSlowly({wrapped}));
}

TEST_P(UnexportableKeyTest, AttestationKeyMock) {
  crypto::ScopedMockUnexportableKeyProvider mock_provider;

  auto mock_attestation_key =
      std::make_unique<crypto::MockUnexportableAttestationKey>();

  EXPECT_CALL(*mock_attestation_key, CertifySlowly)
      .WillOnce(testing::Return(crypto::AttestationStatement{
          .format = crypto::AttestationStatement::kTpm,
          .statement = {1, 2, 3},
          .signature = {4, 5, 6},
      }));

  EXPECT_CALL(mock_provider.mock(), GenerateAttestationKeySlowly)
      .WillOnce(Return(std::move(mock_attestation_key)));

  auto provider = CreateProvider();
  ASSERT_TRUE(provider);

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  ASSERT_TRUE(attestation_key);

  auto software_provider = crypto::GetSoftwareUnsecureUnexportableKeyProvider();
  auto signing_key = software_provider->GenerateSigningKeySlowly(algorithms);
  ASSERT_TRUE(signing_key);

  auto statement = attestation_key->CertifySlowly(
      *signing_key, std::vector<uint8_t>{7, 8, 9});
  ASSERT_TRUE(statement);
  EXPECT_EQ(statement->format, crypto::AttestationStatement::kTpm);
  EXPECT_THAT(statement->statement, ElementsAre(1, 2, 3));
  EXPECT_THAT(statement->signature, ElementsAre(4, 5, 6));
}

TEST_P(UnexportableKeyTest, FakeAttestationWorkflows) {
  // TODO(crbug.com/525047253): This only tests SHA256 hash algos. We should add
  // coverage for SHA1 as well.
  if (provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Test is only for fake provider.";
  }
  crypto::ScopedFakeUnexportableKeyProvider fake;
  auto provider = CreateProvider();
  ASSERT_TRUE(provider);

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};

  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  ASSERT_TRUE(attestation_key);

  auto signing_key = provider->GenerateSigningKeySlowly(algorithms);
  ASSERT_TRUE(signing_key);

  static constexpr auto kChallenge = std::to_array<uint8_t>({1, 2, 3, 4});
  ASSERT_OK_AND_ASSIGN(
      crypto::AttestationStatement statement,
      attestation_key->CertifySlowly(*signing_key, kChallenge));
  EXPECT_EQ(statement.format, crypto::AttestationStatement::kTpm);

  std::vector<uint8_t> fake_resp =
      ConstructFakeTpmResponse(statement.statement, statement.signature);

  // Use C++ type-safe parser.
  EXPECT_THAT(crypto::tpm::ParseCertifyResponse(fake_resp, kChallenge),
              base::test::ValueIs(crypto::tpm::CertifyResponse{
                  .statement = statement.statement,
                  .signature = statement.signature,
              }));

  // Verify the signature using the C++ wrapper directly.
  EXPECT_OK(
      crypto::tpm::VerifySignature(attestation_key->GetSubjectPublicKeyInfo(),
                                   statement.statement, statement.signature));

  std::vector<uint8_t> wrapped_attestation = attestation_key->GetWrappedKey();
  auto loaded_attestation_key =
      provider->FromWrappedAttestationKeySlowly(wrapped_attestation);
  ASSERT_TRUE(loaded_attestation_key);
  EXPECT_EQ(loaded_attestation_key->Algorithm(), algorithm());
}

TEST_P(UnexportableKeyTest, AttestationKeySignFailsForTpmGeneratedValue) {
  if (provider_type() != Provider::kTPM && provider_type() != Provider::kFake) {
    GTEST_SKIP() << "Attestation keys are only supported on TPM or Fake.";
  }

#if BUILDFLAG(IS_APPLE)
  if (provider_type() == Provider::kTPM) {
    GTEST_SKIP() << "Secure Enclave does not have TPM-style restrictions.";
  }
#endif

  std::optional<crypto::ScopedFakeUnexportableKeyProvider> fake;
  if (provider_type() == Provider::kFake) {
    fake.emplace();
  }

  std::unique_ptr<crypto::UnexportableKeyProvider> provider = CreateProvider();
  if (!provider) {
    GTEST_SKIP() << "Skipping test because of lack of provider support.";
  }

  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      algorithm()};
  auto attestation_key = provider->GenerateAttestationKeySlowly(algorithms);
  if (!attestation_key) {
    GTEST_SKIP() << "Skipping test because of lack of attestation key support.";
  }

  auto payload =
      base::ToVector(base::EnumToBigEndian(crypto::tpm::TPM_GENERATED_VALUE));
  payload.insert(payload.end(), {0x01, 0x02, 0x03, 0x04});
  EXPECT_EQ(attestation_key->SignSlowly(payload), std::nullopt);
}

}  // namespace
