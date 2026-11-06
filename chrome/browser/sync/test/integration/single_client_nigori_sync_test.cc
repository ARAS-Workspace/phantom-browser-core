// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/metrics/metrics_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/engine/loopback_server/loopback_server_entity.h"
#include "components/sync/model/crypto/key_derivation_params.h"
#include "components/sync/model/crypto/nigori.h"
#include "components/sync/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/nigori_local_data.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/sync/test/fake_server_nigori_helper.h"
#include "components/sync/test/nigori_test_utils.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "net/base/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using fake_server::GetServerNigori;
using fake_server::SetNigoriInFakeServer;
using syncer::BuildCustomPassphraseNigoriSpecifics;
using syncer::BuildKeystoreNigoriSpecifics;
using syncer::KeyParamsForTesting;
using syncer::KeystoreKeyParamsForTesting;
using syncer::Pbkdf2PassphraseKeyParamsForTesting;
using testing::Eq;
using testing::SizeIs;

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      syncer::NigoriPassKey::ForTesting(), key_params.derivation_params,
      key_params.password);
  return encrypted_data.key_name() == nigori->GetKeyName();
}

class SingleClientNigoriSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientNigoriSyncTest() : SyncTest(SINGLE_CLIENT) {
    if (GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly) {
      scoped_feature_list_.InitWithFeatures(
          {syncer::kReplaceSyncPromosWithSignInPromos,
           switches::kSyncEnableBookmarksInTransportMode},
          {});
    } else {
      // Skip sync-to-signin migration for sync-the-feature tests. This is to
      // avoid the sync state changing between the PRE_ tests.
      scoped_feature_list_.InitAndDisableFeature(
          switches::kMigrateSyncingUserToSignedIn);
    }
  }

  SingleClientNigoriSyncTest(const SingleClientNigoriSyncTest&) = delete;
  SingleClientNigoriSyncTest& operator=(const SingleClientNigoriSyncTest&) =
      delete;

  ~SingleClientNigoriSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  std::vector<variations::ActiveGroupId> GetSyntheticFieldTrials() {
    return g_browser_process->metrics_service()
        ->GetSyntheticTrialRegistry()
        ->GetCurrentSyntheticFieldTrialsForTest();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientNigoriSyncTest,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

class SingleClientNigoriSyncTestWithNotAwaitQuiescence
    : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriSyncTestWithNotAwaitQuiescence() = default;

  SingleClientNigoriSyncTestWithNotAwaitQuiescence(
      const SingleClientNigoriSyncTestWithNotAwaitQuiescence&) = delete;
  SingleClientNigoriSyncTestWithNotAwaitQuiescence& operator=(
      const SingleClientNigoriSyncTestWithNotAwaitQuiescence&) = delete;

  ~SingleClientNigoriSyncTestWithNotAwaitQuiescence() override = default;

  bool TestUsesSelfNotifications() override {
    // This test fixture is used with tests, which expect SetupSync() to be
    // waiting for completion, but not for quiescence, because it can't be
    // achieved and isn't needed.
    return false;
  }
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientNigoriSyncTestWithNotAwaitQuiescence,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

class SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest
    : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest() = default;
  SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest(
      const SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest&) =
      delete;
  SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest& operator=(
      const SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest&) =
      delete;

  ~SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest() override =
      default;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTest,
                       ShouldCommitKeystoreNigoriWhenReceivedDefault) {
  // SetupSync() should make FakeServer send default NigoriSpecifics.
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/40609954): we may want to actually wait for specifics update
  // in fake server. Due to implementation details it's not currently needed.
  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  EXPECT_THAT(
      specifics.encryption_keybag(),
      IsDataEncryptedWith(KeystoreKeyParamsForTesting(keystore_keys.back())));
  EXPECT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE));
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_TRUE(specifics.has_keystore_migration_time());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriSyncTest,
    UnexpectedEncryptedIncrementalUpdateShouldBeDecryptedAndReCommitted) {
  // Init NIGORI with a single encryption key.
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());

  // Despite BOOKMARKS not being an encrypted type, send an update encrypted
  // with the single key known to this client. This happens after SetupSync(),
  // so it's an incremental update.
  ASSERT_FALSE(
      GetSyncService(0)->GetUserSettings()->GetAllEncryptedDataTypes().Has(
          syncer::DataType::BOOKMARKS));
  const std::u16string kTitle = u"Bookmark title";
  const GURL kUrl = GURL("https://g.com");
  std::unique_ptr<syncer::LoopbackServerEntity> bookmark =
      bookmarks_helper::CreateBookmarkServerEntity(kTitle, kUrl);
  bookmark->SetSpecifics(syncer::GetEncryptedBookmarkEntitySpecifics(
      bookmark->GetSpecifics().bookmark(), kKeystoreKeyParams));
  GetFakeServer()->InjectEntity(std::move(bookmark));

  // The client should decrypt the update and re-commit an unencrypted version.
  EXPECT_TRUE(bookmarks_helper::BookmarksTitleChecker(0, kTitle, 1).Wait());
  EXPECT_TRUE(bookmarks_helper::ServerBookmarksEqualityChecker(
                  {{kTitle, kUrl}},
                  /*cryptographer=*/nullptr)
                  .Wait());
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTest,
                       PRE_ShouldRegisterTrustedVaultSyntheticFieldTrial) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));

  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  sync_pb::NigoriSpecifics nigori_specifics = BuildKeystoreNigoriSpecifics(
      /*keybag_keys_params=*/{kKeystoreKeyParams},
      /*keystore_decryptor_params=*/kKeystoreKeyParams,
      /*keystore_key_params=*/kKeystoreKeyParams);

  const std::string kGroupName = "Cohort7_Control";
  sync_pb::TrustedVaultAutoUpgradeExperimentGroup* experiment_group =
      nigori_specifics.mutable_trusted_vault_debug_info()
          ->mutable_auto_upgrade_experiment_group();
  experiment_group->set_cohort(7);
  experiment_group->set_type(
      sync_pb::TrustedVaultAutoUpgradeExperimentGroup::CONTROL);

  SetNigoriInFakeServer(nigori_specifics, GetFakeServer());

  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName, kGroupName));
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTest,
                       ShouldRegisterTrustedVaultSyntheticFieldTrial) {
  // Same as in previous test (PRE_ test).
  const std::string kGroupName = "Cohort7_Control";

  ASSERT_TRUE(SetupClients());

  // Shortly after profile startup, the group should be re-registered
  // automatically.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(ContainsTrialAndGroupName(
      GetSyntheticFieldTrials(),
      syncer::kTrustedVaultAutoUpgradeSyntheticFieldTrialName, kGroupName));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    ShouldBootstrapCrossUserSharingPublicPrivateKeyPairWhenReceivedDefault) {
  ASSERT_TRUE(SetupSync());
  sync_pb::NigoriSpecifics specifics;

  // Commit of specifics with key pair happens during SetupSync().
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  EXPECT_TRUE(specifics.has_cross_user_sharing_public_key());
  EXPECT_TRUE(
      specifics.cross_user_sharing_public_key().has_x25519_public_key());
  EXPECT_TRUE(specifics.cross_user_sharing_public_key().has_version());
  EXPECT_EQ(specifics.cross_user_sharing_public_key().version(), 0);
  EXPECT_THAT(specifics.cross_user_sharing_public_key().x25519_public_key(),
              SizeIs(X25519_PUBLIC_VALUE_LEN));

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  EXPECT_THAT(
      specifics.encryption_keybag(),
      IsDataEncryptedWith(KeystoreKeyParamsForTesting(keystore_keys.back())));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kKeystoreKeyParams.password, kKeystoreKeyParams.derivation_params);

  std::string decrypted_keys_str;
  EXPECT_TRUE(cryptographer->DecryptToString(specifics.encryption_keybag(),
                                             &decrypted_keys_str));
  sync_pb::EncryptionKeys decrypted_keys;

  EXPECT_TRUE(decrypted_keys.ParseFromString(decrypted_keys_str));
  ASSERT_THAT(decrypted_keys.cross_user_sharing_private_key(), SizeIs(1));
  auto private_key_proto = decrypted_keys.cross_user_sharing_private_key()
                               .at(0)
                               .x25519_private_key();
  EXPECT_THAT(private_key_proto, SizeIs(X25519_PRIVATE_KEY_LEN));
  EXPECT_EQ(decrypted_keys.cross_user_sharing_private_key().at(0).version(), 0);
  std::vector<uint8_t> raw_private_key(private_key_proto.begin(),
                                       private_key_proto.end());
  std::optional<base::span<uint8_t, X25519_PRIVATE_KEY_LEN>> fixed_private_key =
      base::span(raw_private_key).to_fixed_extent<X25519_PRIVATE_KEY_LEN>();
  ASSERT_TRUE(fixed_private_key);

  syncer::CrossUserSharingPublicPrivateKeyPair private_key(*fixed_private_key);
  EXPECT_THAT(specifics.cross_user_sharing_public_key().x25519_public_key(),
              testing::ElementsAreArray(private_key.GetRawPublicKey()));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    PRE_ShouldSyncCrossUserSharingPublicPrivateKeyPair) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(keystore_keys, SizeIs(1));
  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  SetNigoriInFakeServer(BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams),
                        GetFakeServer());

  ASSERT_TRUE(SetupSync());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    ShouldSyncCrossUserSharingPublicPrivateKeyPair) {
  ASSERT_TRUE(SetupSync());
  sync_pb::NigoriSpecifics specifics;

  // Commit of specifics with key pair happens during SetupSync().
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  EXPECT_TRUE(specifics.has_cross_user_sharing_public_key());
  EXPECT_TRUE(
      specifics.cross_user_sharing_public_key().has_x25519_public_key());
  EXPECT_TRUE(specifics.cross_user_sharing_public_key().has_version());
  EXPECT_EQ(specifics.cross_user_sharing_public_key().version(), 0);
  EXPECT_THAT(specifics.cross_user_sharing_public_key().x25519_public_key(),
              SizeIs(X25519_PUBLIC_VALUE_LEN));

  const std::vector<std::vector<uint8_t>>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  EXPECT_THAT(
      specifics.encryption_keybag(),
      IsDataEncryptedWith(KeystoreKeyParamsForTesting(keystore_keys.back())));

  const KeyParamsForTesting kKeystoreKeyParams =
      KeystoreKeyParamsForTesting(keystore_keys.back());
  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          kKeystoreKeyParams.password, kKeystoreKeyParams.derivation_params);

  std::string decrypted_keys_str;
  EXPECT_TRUE(cryptographer->DecryptToString(specifics.encryption_keybag(),
                                             &decrypted_keys_str));
  sync_pb::EncryptionKeys decrypted_keys;
  EXPECT_TRUE(decrypted_keys.ParseFromString(decrypted_keys_str));
  ASSERT_THAT(decrypted_keys.cross_user_sharing_private_key(), SizeIs(1));
  auto private_key_proto = decrypted_keys.cross_user_sharing_private_key()
                               .at(0)
                               .x25519_private_key();
  EXPECT_THAT(private_key_proto, SizeIs(X25519_PRIVATE_KEY_LEN));
  EXPECT_EQ(decrypted_keys.cross_user_sharing_private_key().at(0).version(), 0);
  std::vector<uint8_t> raw_private_key(private_key_proto.begin(),
                                       private_key_proto.end());
  std::optional<base::span<uint8_t, X25519_PRIVATE_KEY_LEN>> fixed_private_key =
      base::span(raw_private_key).to_fixed_extent<X25519_PRIVATE_KEY_LEN>();
  ASSERT_TRUE(fixed_private_key);

  syncer::CrossUserSharingPublicPrivateKeyPair private_key(*fixed_private_key);
  EXPECT_THAT(specifics.cross_user_sharing_public_key().x25519_public_key(),
              testing::ElementsAreArray(private_key.GetRawPublicKey()));
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    PRE_ShouldRecreateKeyPairUponClientServerInconsistency) {
  ASSERT_TRUE(SetupSync());
  sync_pb::NigoriSpecifics specifics;

  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  EXPECT_TRUE(specifics.has_cross_user_sharing_public_key());
  EXPECT_TRUE(
      specifics.cross_user_sharing_public_key().has_x25519_public_key());

  // Mimic remote transition to custom passphrase without
  // cross_user_sharing_public_key.
  const KeyParamsForTesting kCustomPassphraseKeyParams =
      Pbkdf2PassphraseKeyParamsForTesting("passphrase");
  SetNigoriInFakeServer(
      BuildCustomPassphraseNigoriSpecifics(kCustomPassphraseKeyParams),
      GetFakeServer());

  EXPECT_TRUE(PassphraseRequiredChecker(GetSyncService(0)).Wait());
  EXPECT_TRUE(GetSyncService(0)->GetUserSettings()->SetDecryptionPassphrase(
      kCustomPassphraseKeyParams.password));
  EXPECT_TRUE(PassphraseAcceptedChecker(GetSyncService(0)).Wait());
}

// Tests that upon an inconsistent state between client and server in which the
// cross-user sharing key-pair is missing on the server, a new cross-user
// sharing key-pair is created on the client and synced to the server.
IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriCrossUserSharingPublicPrivateKeyPairSyncTest,
    ShouldRecreateKeyPairUponClientServerInconsistency) {
  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(ServerCrossUserSharingPublicKeyChangedChecker().Wait());
}

// Performs initial sync for Nigori, but doesn't allow initialized Nigori to be
// committed.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       PRE_ShouldCompleteKeystoreInitializationAfterRestart) {
  GetFakeServer()->TriggerCommitError(sync_pb::SyncEnums::THROTTLED);

  // Do not wait for commits due to commit error.
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));

  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE));
}

// After browser restart the client should commit initialized Nigori.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithNotAwaitQuiescence,
                       ShouldCompleteKeystoreInitializationAfterRestart) {
  sync_pb::NigoriSpecifics specifics;
  ASSERT_TRUE(GetServerNigori(GetFakeServer(), &specifics));
  ASSERT_THAT(specifics.passphrase_type(),
              Eq(sync_pb::NigoriSpecifics::IMPLICIT_PASSPHRASE));

  ASSERT_TRUE(SetupClients());
  EXPECT_TRUE(
      ServerPassphraseTypeChecker(syncer::PassphraseType::kKeystorePassphrase)
          .Wait());
}

}  // namespace
