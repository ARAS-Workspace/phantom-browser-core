// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/standalone_trusted_vault_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#if BUILDFLAG(IS_MAC)
#include "crypto/apple/scoped_fake_keychain_v2.h"
#endif
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {
namespace {

using testing::ElementsAre;
using testing::IsEmpty;

constexpr char kTestEmail[] = "test@gmail.com";

#if BUILDFLAG(IS_MAC)
constexpr char kKeychainAccessGroupPrefix[] = "test-access-group-prefix";
const std::string kKeychainAccessGroup(
    base::StrCat({kKeychainAccessGroupPrefix, ".com.google.common.folsom"}));
#endif

class MockTrustedVaultClientObserver : public TrustedVaultClient::Observer {
 public:
  MockTrustedVaultClientObserver() = default;
  ~MockTrustedVaultClientObserver() override = default;

  MOCK_METHOD(void,
              OnTrustedVaultKeysChanged,
              (std::optional<TrustedVaultUserActionTriggerForUMA>),
              (override));
  MOCK_METHOD(void, OnTrustedVaultRecoverabilityChanged, (), (override));
};

// TESTING BOUNDARY AND CLASSES UNDER TEST:
// This fixture implements multi-class client integration tests that evaluate
// the complete standalone trusted vault client subsystem without a browser
// process.
//
// 1. Classes Under Test:
//    - StandaloneTrustedVaultClient (public client API)
//    - StandaloneTrustedVaultBackend (core engine, running in a sequence)
//    - Persistent disk storage layer
//
// 2. Testing Boundaries:
//    - All test interactions occur through the public TrustedVaultClient
//      interface.
//    - The client is built without a TrustedVaultConnection.
//    - Persistent protobuf serialization (trusted_vault.pb) is exercised
//      against a real disk directory using base::ScopedTempDir.
//
// Note: These tests do not aim to cover all possible edge cases - there are
// more suitable places to do that, e.g. in
// standalone_trusted_vault_backend_unittest.cc. The goal here is to test the
// interaction of the various components (storage, helpers, etc.) for relevant
// user scenarios.
class StandaloneTrustedVaultClientTest : public testing::Test {
 public:
  StandaloneTrustedVaultClientTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~StandaloneTrustedVaultClientTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  void TearDown() override {
    // Make sure to run all pending deletions.
    task_environment_.RunUntilIdle();
  }

  std::unique_ptr<StandaloneTrustedVaultClient> CreateClient() {
    return std::make_unique<StandaloneTrustedVaultClient>(
#if BUILDFLAG(IS_MAC)
        kKeychainAccessGroupPrefix,
#endif
        SecurityDomainId::kChromeSync,
        /*base_dir=*/temp_dir_.GetPath(),
        identity_test_env_.identity_manager());
  }

  CoreAccountInfo MakeAccountAvailable(const std::string& email) {
    return identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
  }

  void WaitForIdle(StandaloneTrustedVaultClient* client) {
    base::test::TestFuture<void> future;
    client->WaitForIdleForTesting(future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  std::vector<std::vector<uint8_t>> FetchKeys(
      StandaloneTrustedVaultClient* client,
      const CoreAccountInfo& account) {
    base::test::TestFuture<const std::vector<std::vector<uint8_t>>&> future;
    client->FetchKeys(account, future.GetCallback());
    return future.Get();
  }

  void StoreKeys(StandaloneTrustedVaultClient* client,
                 const GaiaId& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version) {
    client->StoreKeys(gaia_id, keys, last_key_version,
                      /*trigger=*/std::nullopt);
  }

  bool MarkLocalKeysAsStale(StandaloneTrustedVaultClient* client,
                            const CoreAccountInfo& account) {
    base::test::TestFuture<bool> future;
    client->MarkLocalKeysAsStale(account, future.GetCallback());
    return future.Get();
  }

  void AddTrustedRecoveryMethod(StandaloneTrustedVaultClient* client,
                                const GaiaId& gaia_id,
                                const std::vector<uint8_t>& public_key,
                                int method_type_hint) {
    base::test::TestFuture<void> future;
    client->AddTrustedRecoveryMethod(gaia_id, public_key, method_type_hint,
                                     future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  int GetLastKeyVersion(StandaloneTrustedVaultClient* client,
                        const GaiaId& gaia_id) {
    base::test::TestFuture<int> future;
    client->GetLastKeyVersionForTesting(gaia_id, future.GetCallback());
    return future.Get();
  }

 protected:
#if BUILDFLAG(IS_MAC)
  crypto::apple::ScopedFakeKeychainV2 fake_keychain_{kKeychainAccessGroup};
#endif
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldKeepKeysLocallyWithoutConnection) {
  const std::vector<uint8_t> kKey = {1, 2, 3, 4};

  std::unique_ptr<StandaloneTrustedVaultClient> client = CreateClient();
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);
  WaitForIdle(client.get());
  EXPECT_THAT(FetchKeys(client.get(), account_info), IsEmpty());

  MockTrustedVaultClientObserver observer;
  client->AddObserver(&observer);

  EXPECT_CALL(observer, OnTrustedVaultKeysChanged);
  StoreKeys(client.get(), account_info.gaia, {kKey}, /*last_key_version=*/1);
  WaitForIdle(client.get());
  EXPECT_THAT(FetchKeys(client.get(), account_info), ElementsAre(kKey));

  // Stale keys are still returned from local storage.
  EXPECT_TRUE(MarkLocalKeysAsStale(client.get(), account_info));
  EXPECT_THAT(FetchKeys(client.get(), account_info), ElementsAre(kKey));

  std::unique_ptr<SecureBoxKeyPair> key_pair =
      SecureBoxKeyPair::GenerateRandom();
  AddTrustedRecoveryMethod(client.get(), account_info.gaia,
                           key_pair->public_key().ExportToBytes(),
                           /*method_type_hint=*/3);

  client->RemoveObserver(&observer);
}

TEST_F(StandaloneTrustedVaultClientTest,
       ShouldReadKeysAndStateFromDiskOnClientRestart) {
  const std::vector<uint8_t> kKey = {5, 6, 7, 8};
  const int kServerEpoch = 10;
  CoreAccountInfo account_info = MakeAccountAvailable(kTestEmail);

  {
    std::unique_ptr<StandaloneTrustedVaultClient> client_a = CreateClient();
    StoreKeys(client_a.get(), account_info.gaia, {kKey}, kServerEpoch);
    WaitForIdle(client_a.get());
  }

  {
    std::unique_ptr<StandaloneTrustedVaultClient> client_b = CreateClient();
    EXPECT_THAT(FetchKeys(client_b.get(), account_info), ElementsAre(kKey));
    EXPECT_EQ(GetLastKeyVersion(client_b.get(), account_info.gaia),
              kServerEpoch);
  }
}

}  // namespace
}  // namespace trusted_vault
