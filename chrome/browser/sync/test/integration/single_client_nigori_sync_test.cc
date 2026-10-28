// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/cookie_helper.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/trusted_vault/trusted_vault_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/grit/generated_resources.h"
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
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/features.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/standalone_trusted_vault_client.h"
#include "components/trusted_vault/standalone_trusted_vault_server_constants.h"
#include "components/trusted_vault/test/fake_security_domains_server.h"
#include "components/trusted_vault/trusted_vault_client.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "components/trusted_vault/trusted_vault_service.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/variations_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_switches.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_constants.h"

namespace {

using fake_server::GetServerNigori;
using fake_server::SetNigoriInFakeServer;
using syncer::BuildCustomPassphraseNigoriSpecifics;
using syncer::BuildKeystoreNigoriSpecifics;
using syncer::BuildTrustedVaultNigoriSpecifics;
using syncer::KeyParamsForTesting;
using syncer::KeystoreKeyParamsForTesting;
using syncer::Pbkdf2PassphraseKeyParamsForTesting;
using testing::Eq;
using testing::NotNull;
using testing::SizeIs;

// This constant matches SyncSigninDelegate's internal implementation when using
// fake accounts. Ideally it shouldn't be hardcoded here and instead the fake
// server that implements the retrieval page should be able to determine the
// gaia ID from cookies, but this is currently not implemented.
constexpr GaiaId::Literal kDefaultGaiaId("gaia_id_for_user1_gmail.com");

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      syncer::NigoriPassKey::ForTesting(), key_params.derivation_params,
      key_params.password);
  return encrypted_data.key_name() == nigori->GetKeyName();
}

class WifiConfigurationsSyncActiveChecker
    : public SingleClientStatusChangeChecker {
 public:
  explicit WifiConfigurationsSyncActiveChecker(
      syncer::SyncServiceImpl* sync_service)
      : SingleClientStatusChangeChecker(sync_service) {}
  ~WifiConfigurationsSyncActiveChecker() override = default;

  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for WIFI_CONFIGURATIONS sync to become active";
    return service()->GetActiveDataTypes().Has(syncer::WIFI_CONFIGURATIONS);
  }
};

// Used to wait until a tab closes.
class TabClosedChecker : public StatusChangeChecker,
                         public content::WebContentsObserver {
 public:
  explicit TabClosedChecker(content::WebContents* web_contents)
      : WebContentsObserver(web_contents) {
    DCHECK(web_contents);
  }

  TabClosedChecker(const TabClosedChecker&) = delete;
  TabClosedChecker& operator=(const TabClosedChecker&) = delete;

  ~TabClosedChecker() override = default;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for the tab to be closed";
    return closed_;
  }

  // content::WebContentsObserver overrides.
  void WebContentsDestroyed() override {
    closed_ = true;
    CheckExitCondition();
  }

 private:
  bool closed_ = false;
};

class FakeSecurityDomainsServerMemberStatusChecker
    : public StatusChangeChecker,
      public trusted_vault::FakeSecurityDomainsServer::Observer {
 public:
  FakeSecurityDomainsServerMemberStatusChecker(
      int expected_member_count,
      const std::vector<uint8_t>& expected_trusted_vault_key,
      trusted_vault::FakeSecurityDomainsServer* server)
      : expected_member_count_(expected_member_count),
        expected_trusted_vault_key_(expected_trusted_vault_key),
        server_(server) {
    server_->AddObserver(this);
  }

  ~FakeSecurityDomainsServerMemberStatusChecker() override {
    server_->RemoveObserver(this);
  }

 protected:
  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for security domains server to have members with"
           " expected key.";
    if (server_->GetMemberCount() != expected_member_count_) {
      *os << "Security domains server member count ("
          << server_->GetMemberCount() << ") doesn't match expected value ("
          << expected_member_count_ << ").";
      return false;
    }
    if (!server_->AllMembersHaveKey(expected_trusted_vault_key_)) {
      *os << "Some members in security domains service don't have expected "
             "key.";
      return false;
    }
    return true;
  }

 private:
  // FakeSecurityDomainsServer::Observer implementation.
  void OnRequestHandled() override { CheckExitCondition(); }

  int expected_member_count_;
  std::vector<uint8_t> expected_trusted_vault_key_;
  const raw_ptr<trusted_vault::FakeSecurityDomainsServer> server_;
};

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

class SingleClientNigoriWithWebApiTest : public SingleClientNigoriSyncTest {
 public:
  SingleClientNigoriWithWebApiTest() = default;

  SingleClientNigoriWithWebApiTest(const SingleClientNigoriWithWebApiTest&) =
      delete;
  SingleClientNigoriWithWebApiTest& operator=(
      const SingleClientNigoriWithWebApiTest&) = delete;

  ~SingleClientNigoriWithWebApiTest() override = default;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ASSERT_TRUE(embedded_https_test_server().InitializeAndListen());
    const GURL& base_url = embedded_https_test_server().base_url();
    command_line->AppendSwitchASCII(switches::kGaiaUrl, base_url.spec());
    command_line->AppendSwitchASCII(
        trusted_vault::kTrustedVaultServiceURLSwitch,
        trusted_vault::FakeSecurityDomainsServer::GetServerURL(
            embedded_https_test_server().base_url())
            .spec());

    SingleClientNigoriSyncTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    SingleClientNigoriSyncTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    security_domains_server_ =
        std::make_unique<trusted_vault::FakeSecurityDomainsServer>(
            embedded_https_test_server().base_url());
    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &trusted_vault::FakeSecurityDomainsServer::HandleRequest,
        base::Unretained(security_domains_server_.get())));

    encryption_helper::SetupFakeTrustedVaultPages(
        kDefaultGaiaId, kTestEncryptionKey, kTestEncryptionKeyVersion,
        kTestRecoveryMethodPublicKey, &embedded_https_test_server());

    embedded_https_test_server().StartAcceptingConnections();
  }

  void TearDown() override {
    // Test server shutdown is required before |security_domains_server_| can be
    // destroyed.
    ASSERT_TRUE(embedded_https_test_server().ShutdownAndWaitUntilComplete());
    SingleClientNigoriSyncTest::TearDown();
  }

  trusted_vault::FakeSecurityDomainsServer* GetSecurityDomainsServer() {
    return security_domains_server_.get();
  }

  trusted_vault::TrustedVaultClient* GetSyncTrustedVaultClient() {
    return TrustedVaultServiceFactory::GetForProfile(GetProfile(0))
        ->GetTrustedVaultClient(trusted_vault::SecurityDomainId::kChromeSync);
  }

 protected:
  // Arbitrary encryption key that the Gaia retrieval page returns via
  // Javascript API if the retrieval page is visited.
  const std::vector<uint8_t> kTestEncryptionKey = {1, 2, 3, 4};
  const int kTestEncryptionKeyVersion = 23;

  // Arbitrary (but valid) public key of a recovery method that gets
  // automatically added if the Gaia recoverability page is visited.
  const std::vector<uint8_t> kTestRecoveryMethodPublicKey =
      trusted_vault::SecureBoxKeyPair::GenerateRandom()
          ->public_key()
          .ExportToBytes();

 private:
  std::unique_ptr<trusted_vault::FakeSecurityDomainsServer>
      security_domains_server_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientNigoriWithWebApiTest,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

// TODO(crbug.com/40276245): Some changes desired once test confirmed to be
// deflaked:
// 1. ShouldRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown does almost
// the same, but have unique expectation. Consider to dedup them.
// 2. BeforeSignIn is misleading (SetupClients() *does* sign in), either rename
// the test to reflect this or change it (likely we need some helper that
// creates the profile, but doesn't sign in). Same applies to comments in both
// tests.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriWithWebApiTest,
                       PRE_ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes and keys are persisted.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  base::RunLoop run_loop;
  static_cast<trusted_vault::StandaloneTrustedVaultClient*>(
      GetSyncTrustedVaultClient())
      ->WaitForIdleForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriWithWebApiTest,
                       ShouldAcceptEncryptionKeysFromTheWebBeforeSignIn) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  EXPECT_TRUE(SetupSync());

  ASSERT_THAT(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
              Eq(syncer::PassphraseType::kTrustedVaultPassphrase));
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultRecoverabilityDegraded());

  // Verify the profile-menu error string is empty.
  EXPECT_EQ(GetSyncService(0)->GetUserActionableError(),
            syncer::SyncService::UserActionableError::kNone);
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    PRE_ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  // TODO(crbug.com/40276245): TrustedVaultKeysChangedStateChecker may be not
  // sufficient and redundant in this test, consider rewriting it using
  // StandaloneTrustedVaultClient::WaitForIdleForTesting().
  ASSERT_TRUE(SetupClients());

  // Explicitly add signin cookie (normally it would be done during the keys
  // retrieval or before it).
  cookie_helper::AddSigninCookie(GetProfile(0));

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  TrustedVaultKeysChangedStateChecker keys_fetched_checker(GetSyncService(0));
  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes, which indicates successful completion.
  EXPECT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  EXPECT_TRUE(keys_fetched_checker.Wait());

  // TrustedVaultClient handles IdentityManager state changes after refresh
  // tokens are loaded.
  // TODO(crbug.com/40156992): |keys_cleared_checker| should be sufficient alone
  // once test properly manipulates AccountsInCookieJarInfo (this likely
  // involves using FakeGaia).
  signin::WaitForRefreshTokensLoaded(
      IdentityManagerFactory::GetForProfile(GetProfile(0)));

  // Mimic signin cookie clearing.
  TrustedVaultKeysChangedStateChecker keys_cleared_checker(GetSyncService(0));
  cookie_helper::DeleteSigninCookies(GetProfile(0));
  EXPECT_TRUE(keys_cleared_checker.Wait());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldClearEncryptionKeysFromTheWebWhenSigninCookiesCleared) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  // Sign in and start sync.
  ASSERT_TRUE(SetupSync());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldRecordTrustedVaultErrorShownOnStartupWhenErrorShown) {
  // 4 days is an arbitrary value between 3 days and 7 days to allow testing
  // histogram suffixes.
  const base::Time migration_time = base::Time::Now() - base::Days(4);

  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(
      BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}, migration_time),
      GetFakeServer());

  base::HistogramTester histogram_tester;

  ASSERT_TRUE(SignIn());
  ASSERT_TRUE(SetupSync(WAIT_FOR_SYNC_SETUP_TO_COMPLETE));

  ASSERT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultKeyRequiredForPreferredDataTypes());

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast28Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast7Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLast3Days",
      /*count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultErrorShownOnStartup.MigratedLastDay",
      /*count=*/0);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultErrorShownOnFirstTimeSync2",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    PRE_ShouldRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown) {
  ASSERT_TRUE(SetupClients());

  // There needs to be an existing tab for the second tab (the retrieval flow)
  // to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);

  // Mimic opening a web page where the user can interact with the retrieval
  // flow, while the user is signed out.
  OpenTabForSyncKeyRetrieval(
      GetBrowser(0),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  // Wait until the page closes and keys are persisted.
  ASSERT_TRUE(
      TabClosedChecker(GetBrowser(0)->tab_strip_model()->GetActiveWebContents())
          .Wait());
  base::RunLoop run_loop;
  static_cast<trusted_vault::StandaloneTrustedVaultClient*>(
      GetSyncTrustedVaultClient())
      ->WaitForIdleForTesting(run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldRecordTrustedVaultErrorShownOnStartupWhenErrorNotShown) {
  // Mimic the account being already using a trusted vault passphrase.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics({kTestEncryptionKey}),
                        GetFakeServer());

  base::HistogramTester histogram_tester;
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());

  histogram_tester.ExpectUniqueSample("Sync.TrustedVaultErrorShownOnStartup",
                                      /*sample=*/false,
                                      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(SingleClientNigoriWithWebApiTest,
                       ShouldReportDegradedTrustedVaultRecoverability) {
  base::HistogramTester histogram_tester;

  // 4 days is an arbitrary value between 3 days and 7 days to allow testing
  // histogram suffixes.
  const base::Time migration_time = base::Time::Now() - base::Days(4);

  // Mimic the key being available upon startup but recoverability degraded.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(
      BuildTrustedVaultNigoriSpecifics(
          /*trusted_vault_keys=*/{trusted_vault_key}, migration_time),
      GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncTrustedVaultClient()->StoreKeys(
      kDefaultGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch(),
      /*trigger=*/std::nullopt);
  ASSERT_TRUE(SetupSync());

  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  EXPECT_TRUE(GetSyncService(0)
                  ->GetUserSettings()
                  ->IsTrustedVaultRecoverabilityDegraded());

  ASSERT_THAT(GetSyncService(0)->GetUserSettings()->GetPassphraseType(),
              Eq(syncer::PassphraseType::kTrustedVaultPassphrase));
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultKeyRequiredForPreferredDataTypes());

  // Verify the profile-menu error string.
  EXPECT_THAT(GetSyncService(0)->GetUserActionableError(),
              Eq(syncer::SyncService::UserActionableError::
                     kTrustedVaultRecoverabilityDegradedForPasswords));

  // Mimic opening a web page where the user can interact with the degraded
  // recoverability flow. Before that, there needs to be an existing tab for the
  // second tab to be closeable via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  OpenTabForSyncKeyRecoverabilityDegraded(
      GetBrowser(0),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  ASSERT_THAT(GetBrowser(0)->tab_strip_model()->GetActiveWebContents(),
              NotNull());

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultRecoverabilityDegraded());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

  // Verify the profile-menu error string is empty.
  EXPECT_EQ(GetSyncService(0)->GetUserActionableError(),
            syncer::SyncService::UserActionableError::kNone);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast28Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast7Days",
      /*sample=*/true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLast3Days",
      /*count=*/0);
  histogram_tester.ExpectTotalCount(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup.MigratedLastDay",
      /*count=*/0);

  // TODO(crbug.com/40178774): Verify the recovery method hint added to the fake
  // server.
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldDeferAddingTrustedVaultRecoverabilityMethodUntilSignIn) {
  const int kTestMethodTypeHint = 8;

  // Mimic the account being already using a trusted vault passphrase.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());

  // Mimic the key being available upon startup but recoverability degraded.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  GetSyncTrustedVaultClient()->StoreKeys(
      kDefaultGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch(),
      /*trigger=*/std::nullopt);

  // Mimic a recovery method being added before or during sign-in, which should
  // be deferred until sign-in completes.
  base::RunLoop run_loop;
  GetSyncTrustedVaultClient()->AddTrustedRecoveryMethod(
      kDefaultGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());

  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

  // Sign in now and wait until sync initializes.
  ASSERT_TRUE(SetupSync());

  // Wait until AddTrustedRecoveryMethod() completes.
  run_loop.Run();

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldDeferAddingTrustedVaultRecoverabilityMethodUntilAuthErrorFixed) {
  const int kTestMethodTypeHint = 8;

  // Mimic the account being already using a trusted vault passphrase.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());

  // Mimic the key being available upon startup but recoverability degraded.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  GetSyncTrustedVaultClient()->StoreKeys(
      kDefaultGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch(),
      /*trigger=*/std::nullopt);
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());

  // Sign in now and wait until sync initializes.
  ASSERT_TRUE(SetupSync());

  // Enter a persistent auth error state.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  }
  ASSERT_TRUE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Mimic a recovery method being added during a persistent auth error, which
  // should be deferred until the auth error is resolved.
  base::RunLoop run_loop;
  GetSyncTrustedVaultClient()->AddTrustedRecoveryMethod(
      kDefaultGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());

  // Mimic the auth error state being resolved.
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  }
  ASSERT_FALSE(GetSyncService(0)->GetAuthError().IsPersistentError());

  // Wait until AddTrustedRecoveryMethod() completes.
  run_loop.Run();

  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldReportDegradedTrustedVaultRecoverabilityUponResolvedAuthError) {
  // Mimic the key being available upon startup and recoverability good (not
  // degraded).
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncTrustedVaultClient()->StoreKeys(
      kDefaultGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch(),
      /*trigger=*/std::nullopt);
  ASSERT_TRUE(SetupSync());
  ASSERT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  ASSERT_FALSE(GetSyncService(0)
                   ->GetUserSettings()
                   ->IsTrustedVaultRecoverabilityDegraded());

  // Mimic a server-side persistent auth error together with a degraded
  // recoverability, such as an account recovery flow that resets the account
  // password.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManagerFactory::GetForProfile(GetProfile(0)),
      GetSyncService(0)->GetAccountInfo().account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);

  // Mimic resolving the auth error (e.g. user reauth).
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      IdentityManagerFactory::GetForProfile(GetProfile(0)),
      GetSyncService(0)->GetAccountInfo().account_id, GoogleServiceAuthError());

  // The recoverability state should be immediately refreshed.
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());
}

// Device registration attempt should be taken upon sign in into primary
// profile. It should be successful when security domain server allows device
// registration with constant key.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriWithWebApiTest,
                       ShouldRegisterDeviceWithConstantKey) {
  ASSERT_TRUE(SetupSync());
  // TODO(crbug.com/40143545): consider checking member public key (requires
  // either ability to overload key generator in the test or exposing public key
  // from the client).
  EXPECT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->ReceivedInvalidRequest());
}

// Regression test for crbug.com/40802753: after following key rotation the
// client should still send all trusted vault keys (including keys that predate
// key rotation) to the server when adding recovery method.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriWithWebApiTest,
                       ShouldFollowKeyRotationAndAddRecoveryMethod) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(FakeSecurityDomainsServerMemberStatusChecker(
                  /*expected_member_count=*/1,
                  /*expected_trusted_vault_key=*/
                  trusted_vault::GetConstantTrustedVaultKey(),
                  GetSecurityDomainsServer())
                  .Wait());

  std::vector<uint8_t> new_trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  // Trigger following key rotation client-side.
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{new_trusted_vault_key}),
                        GetFakeServer());

  const int kTestMethodTypeHint = 8;

  // Enter degraded recoverability state.
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  ASSERT_TRUE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
  ASSERT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // Mimic a recovery method being added.
  base::RunLoop run_loop;
  GetSyncTrustedVaultClient()->AddTrustedRecoveryMethod(
      kDefaultGaiaId, kTestRecoveryMethodPublicKey, kTestMethodTypeHint,
      run_loop.QuitClosure());
  run_loop.Run();

  // Verify that recovery method was added. Server rejects the request if client
  // didn't send all keys.
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());
  EXPECT_FALSE(GetSecurityDomainsServer()->IsRecoverabilityDegraded());
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriSyncTest,
    ShouldPauseSyncForEncryptedTypesWhenKeystoreKeysRequired) {
  const std::vector<std::vector<uint8_t>>& server_keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_THAT(server_keystore_keys, SizeIs(1));

  std::vector<uint8_t> wrong_keystore_key = server_keystore_keys[0];
  wrong_keystore_key[0] ^= 0xFF;

  const KeyParamsForTesting kWrongKeystoreKeyParams =
      KeystoreKeyParamsForTesting(wrong_keystore_key);

  SetNigoriInFakeServer(
      BuildKeystoreNigoriSpecifics(
          /*keybag_keys_params=*/{kWrongKeystoreKeyParams},
          /*keystore_decryptor_params=*/kWrongKeystoreKeyParams,
          /*keystore_key_params=*/kWrongKeystoreKeyParams),
      GetFakeServer());

  const std::u16string kBookmarkTitle = u"Bookmark title";
  const GURL kBookmarkUrl = GURL("https://example.com");
  GetFakeServer()->InjectEntity(bookmarks_helper::CreateBookmarkServerEntity(
      kBookmarkTitle, kBookmarkUrl));

  ASSERT_TRUE(SetupSync(NO_WAITING));

  EXPECT_TRUE(KeystoreKeysRequiredChecker(GetSyncService(0)).Wait());

  // Bookmarks are unencrypted and should continue to sync.
  EXPECT_TRUE(
      bookmarks_helper::BookmarksTitleChecker(0, kBookmarkTitle, 1).Wait());

  // The user-facing error should NOT indicate that a passphrase is required,
  // nor should it show any user-actionable errors.
  EXPECT_FALSE(GetSyncService(0)->GetUserSettings()->IsPassphraseRequired());
  EXPECT_EQ(GetSyncService(0)->GetUserActionableError(),
            syncer::SyncService::UserActionableError::kNone);
}

IN_PROC_BROWSER_TEST_P(
    SingleClientNigoriWithWebApiTest,
    ShouldReportDegradedTrustedVaultRecoverabilityInTransportMode) {
  base::HistogramTester histogram_tester;

  // Mimic the key being available upon startup but recoverability degraded.
  const std::vector<uint8_t> trusted_vault_key =
      GetSecurityDomainsServer()->RotateTrustedVaultKey(
          /*last_trusted_vault_key=*/trusted_vault::
              GetConstantTrustedVaultKey());
  GetSecurityDomainsServer()->RequirePublicKeyToAvoidRecoverabilityDegraded(
      kTestRecoveryMethodPublicKey);
  SetNigoriInFakeServer(BuildTrustedVaultNigoriSpecifics(
                            /*trusted_vault_keys=*/{trusted_vault_key}),
                        GetFakeServer());
  ASSERT_TRUE(SetupClients());
  GetSyncTrustedVaultClient()->StoreKeys(
      kDefaultGaiaId, GetSecurityDomainsServer()->GetAllTrustedVaultKeys(),
      /*last_key_version=*/GetSecurityDomainsServer()->GetCurrentEpoch(),
      /*trigger=*/std::nullopt);

  ASSERT_TRUE(SignIn());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());

  ASSERT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/true)
                  .Wait());

  // The error is now shown, because an encrypted data type is trying to sync.
  ASSERT_THAT(GetSyncService(0)->GetUserActionableError(),
              Eq(syncer::SyncService::UserActionableError::
                     kTrustedVaultRecoverabilityDegradedForPasswords));

  // Let's resolve the error. Mimic opening a web page where the user would
  // interact with the degraded recoverability flow. Add an extra tab so the
  // flow tab can be closed via javascript.
  chrome::AddTabAt(GetBrowser(0), GURL(url::kAboutBlankURL), /*index=*/0,
                   /*foreground=*/true);
  OpenTabForSyncKeyRecoverabilityDegraded(
      GetBrowser(0),
      trusted_vault::TrustedVaultUserActionTriggerForUMA::kProfileMenu);
  EXPECT_TRUE(TrustedVaultRecoverabilityDegradedStateChecker(GetSyncService(0),
                                                             /*degraded=*/false)
                  .Wait());

  // The error should have disappeared.
  EXPECT_EQ(GetSyncService(0)->GetUserActionableError(),
            syncer::SyncService::UserActionableError::kNone);

  histogram_tester.ExpectUniqueSample(
      "Sync.TrustedVaultRecoverabilityDegradedOnStartup",
      /*sample=*/true, /*expected_bucket_count=*/1);
}

}  // namespace
