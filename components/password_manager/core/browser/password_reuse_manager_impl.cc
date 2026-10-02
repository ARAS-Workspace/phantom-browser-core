// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_reuse_manager_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/autofill/core/common/save_password_progress_logger.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_reuse_detector.h"
#include "components/password_manager/core/browser/password_reuse_manager_signin_notifier.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "google_apis/gaia/gaia_auth_util.h"

using base::RecordAction;
using base::UserMetricsAction;

namespace password_manager {

namespace {

// Represents a single CheckReuse() request. Implements functionality to
// listen to reuse events and propagate them to |consumer| on the sequence on
// which CheckReuseRequest is created.
class CheckReuseRequest final : public PasswordReuseDetectorConsumer {
 public:
  // |consumer| must not be null.
  explicit CheckReuseRequest(
      base::WeakPtr<PasswordReuseDetectorConsumer> consumer);
  ~CheckReuseRequest() override;

  CheckReuseRequest(const CheckReuseRequest&) = delete;
  CheckReuseRequest& operator=(const CheckReuseRequest&) = delete;

  // PasswordReuseDetectorConsumer
  void OnReuseCheckDone(
      bool is_reuse_found,
      size_t password_length,
      std::optional<PasswordHashData> reused_protected_password_hash,
      const std::vector<MatchingReusedCredential>& matching_reused_credentials,
      int saved_passwords,
      const std::string& domain,
      uint64_t reused_password_hash) override;

  base::WeakPtr<PasswordReuseDetectorConsumer> AsWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_;
  const base::WeakPtr<PasswordReuseDetectorConsumer> consumer_weak_;
  base::WeakPtrFactory<CheckReuseRequest> weak_ptr_factory_{this};
};

CheckReuseRequest::CheckReuseRequest(
    base::WeakPtr<PasswordReuseDetectorConsumer> consumer)
    : origin_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      consumer_weak_(std::move(consumer)) {}

CheckReuseRequest::~CheckReuseRequest() = default;

void CheckReuseRequest::OnReuseCheckDone(
    bool is_reuse_found,
    size_t password_length,
    std::optional<PasswordHashData> reused_protected_password_hash,
    const std::vector<MatchingReusedCredential>& matching_reused_credentials,
    int saved_passwords,
    const std::string& domain,
    uint64_t reused_password_hash) {
  origin_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordReuseDetectorConsumer::OnReuseCheckDone,
                     consumer_weak_, is_reuse_found, password_length,
                     reused_protected_password_hash,
                     matching_reused_credentials, saved_passwords, domain,
                     reused_password_hash));
}

void CheckReuseHelper(std::unique_ptr<CheckReuseRequest> request,
                      const std::u16string& input,
                      const std::string& domain,
                      PasswordReuseDetector* reuse_detector) {
  reuse_detector->CheckReuse(input, domain, request.get());
}

}  // namespace

PasswordReuseManagerImpl::PasswordReuseManagerImpl(
    os_crypt_async::OSCryptAsync* os_crypt_async) {
  os_crypt_async->GetInstance(
      base::BindOnce(&PasswordReuseManagerImpl::OnOsCryptAsyncReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

PasswordReuseManagerImpl::~PasswordReuseManagerImpl() = default;

void PasswordReuseManagerImpl::Shutdown() {
  pending_tasks_.clear();
  profile_store_observation_.Reset();
  profile_store_.reset();
  account_store_observation_.Reset();
  account_store_.reset();
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;
  if (notifier_) {
    notifier_->UnsubscribeFromSigninEvents();
  }

  if (reuse_detector_) {
    background_task_runner_->DeleteSoon(FROM_HERE, std::move(reuse_detector_));
  }
}

void PasswordReuseManagerImpl::Init(
    PrefService* prefs,
    PrefService* local_prefs,
    PasswordStoreInterface* profile_store,
    PasswordStoreInterface* account_store,
    std::unique_ptr<PasswordReuseDetector> password_reuse_detector,
    signin::IdentityManager* identity_manager,
    std::unique_ptr<SharedPreferencesDelegate> shared_pref_delegate) {
  prefs_ = prefs;
  InitHashPasswordManager(local_prefs);
  identity_manager_ = identity_manager;
  main_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  DCHECK(main_task_runner_);

  background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE});
  DCHECK(background_task_runner_);
  DCHECK(profile_store);

  reuse_detector_ = std::move(password_reuse_detector);

  account_store_ = account_store;
  profile_store_ = profile_store;
  RequestLoginsFromStores();
}

void PasswordReuseManagerImpl::ReportMetrics(const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::ReportMetrics, username)) {
    return;
  }
  if (username.empty()) {
    return;
  }

  auto hash_password_state =
      hash_password_manager_->HasPasswordHash(username,
                                              /*is_gaia_password=*/true)
          ? metrics_util::IsSyncPasswordHashSaved::SAVED_VIA_LIST_PREF
          : metrics_util::IsSyncPasswordHashSaved::NOT_SAVED;
  metrics_util::LogIsSyncPasswordHashSaved(hash_password_state);
}

void PasswordReuseManagerImpl::CheckReuse(
    const std::u16string& input,
    const std::string& domain,
    PasswordReuseDetectorConsumer* consumer) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  CheckReuseImpl(input, domain, consumer->AsWeakPtr());
}

void PasswordReuseManagerImpl::CheckReuseImpl(
    const std::u16string& input,
    const std::string& domain,
    base::WeakPtr<PasswordReuseDetectorConsumer> consumer) {
  CHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::CheckReuseImpl, input, domain,
                      consumer)) {
    return;
  }
  if (!consumer) {
    return;
  }
  if (!reuse_detector_) {
    consumer->OnReuseCheckDone(false, 0, std::nullopt, {}, 0, std::string(), 0);
    return;
  }
  ScheduleTask(base::BindOnce(
      &CheckReuseHelper, std::make_unique<CheckReuseRequest>(consumer), input,
      domain, base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::PreparePasswordHashData(
    std::optional<metrics_util::SignInState> sign_in_state_for_metrics) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::PreparePasswordHashData,
                      sign_in_state_for_metrics)) {
    return;
  }
  SchedulePasswordHashUpdate(sign_in_state_for_metrics);
  ScheduleEnterprisePasswordURLUpdate();
}

void PasswordReuseManagerImpl::SaveGaiaPasswordHash(
    const std::string& username,
    const std::u16string& password,
    bool is_sync_password_for_metrics,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::SaveGaiaPasswordHash, username,
                      password, is_sync_password_for_metrics, event)) {
    return;
  }
  RecordAction(
      UserMetricsAction("PasswordProtection.Gaia.HashedPasswordSaved"));
  SaveProtectedPasswordHash(username, password, is_sync_password_for_metrics,
                            /*is_gaia_password=*/true, event);
}

void PasswordReuseManagerImpl::SaveEnterprisePasswordHash(
    const std::string& username,
    const std::u16string& password) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::SaveEnterprisePasswordHash,
                      username, password)) {
    return;
  }
  RecordAction(UserMetricsAction(
      "PasswordProtection.NonGaiaEnterprise.HashedPasswordSaved"));
  SaveProtectedPasswordHash(username, password,
                            /*is_sync_password_for_metrics=*/false,
                            /*is_gaia_password=*/false,
                            metrics_util::GaiaPasswordHashChange::
                                NON_GAIA_ENTERPRISE_PASSWORD_CHANGE);
}

void PasswordReuseManagerImpl::SaveProtectedPasswordHash(
    const std::string& username,
    const std::u16string& password,
    bool is_sync_password_for_metrics,
    bool is_gaia_password,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  CHECK(hash_password_manager_);
  if (hash_password_manager_->SavePasswordHash(username, password,
                                               is_gaia_password)) {
    if (is_gaia_password) {
      metrics_util::LogGaiaPasswordHashChange(event,
                                              is_sync_password_for_metrics);
    }
    // This method is not being called on startup so it shouldn't log metrics.
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/std::nullopt);
  }
}

void PasswordReuseManagerImpl::SaveSyncPasswordHash(
    const PasswordHashData& sync_password_data,
    metrics_util::GaiaPasswordHashChange event) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::SaveSyncPasswordHash,
                      sync_password_data, event)) {
    return;
  }
  if (hash_password_manager_->SavePasswordHash(sync_password_data)) {
    metrics_util::LogGaiaPasswordHashChange(event,
                                            /*is_sync_password=*/true);
    SchedulePasswordHashUpdate(/*sign_in_state_for_metrics=*/std::nullopt);
  }
}

void PasswordReuseManagerImpl::ClearGaiaPasswordHash(
    const std::string& username) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::ClearGaiaPasswordHash,
                      username)) {
    return;
  }
  hash_password_manager_->ClearSavedPasswordHash(username,
                                                 /*is_gaia_password=*/true);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get()),
                              username));
}

void PasswordReuseManagerImpl::ClearAllGaiaPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(&PasswordReuseManagerImpl::ClearAllGaiaPasswordHash)) {
    return;
  }
  hash_password_manager_->ClearAllPasswordHash(/* is_gaia_password= */ true);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::ClearAllGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllEnterprisePasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(
          &PasswordReuseManagerImpl::ClearAllEnterprisePasswordHash)) {
    return;
  }
  hash_password_manager_->ClearAllPasswordHash(/* is_gaia_password= */ false);
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllEnterprisePasswordHash,
                     base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::ClearAllNonGmailPasswordHash() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(
          &PasswordReuseManagerImpl::ClearAllNonGmailPasswordHash)) {
    return;
  }
  hash_password_manager_->ClearAllNonGmailPasswordHash();
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearAllNonGmailPasswordHash,
                     base::Unretained(reuse_detector_.get())));
}

void PasswordReuseManagerImpl::SetPasswordReuseManagerSigninNotifier(
    std::unique_ptr<PasswordReuseManagerSigninNotifier> notifier) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (!hash_password_manager_) {
    CHECK(DelayUntilReady(
        &PasswordReuseManagerImpl::SetPasswordReuseManagerSigninNotifier,
        std::move(notifier)));
    return;
  }
  DCHECK(!notifier_);
  DCHECK(notifier);
  notifier_ = std::move(notifier);
  notifier_->SubscribeToSigninEvents(this);
}

void PasswordReuseManagerImpl::InitHashPasswordManager(
    PrefService* local_prefs) {
  if (DelayUntilReady(&PasswordReuseManagerImpl::InitHashPasswordManager,
                      base::Unretained(local_prefs))) {
    return;
  }
  hash_password_manager_->set_prefs(prefs_);
  hash_password_manager_->set_local_prefs(local_prefs);
  hash_password_manager_->MigrateEnterprisePasswordHashes();
}

void PasswordReuseManagerImpl::OnOsCryptAsyncReady(
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  hash_password_manager_ =
      std::make_unique<HashPasswordManager>(std::move(encryptor));
  state_callback_list_subscription_ =
      hash_password_manager_->RegisterStateCallback(base::BindRepeating(
          &PasswordReuseManagerImpl::HashPasswordManagerStateChanged,
          weak_ptr_factory_.GetWeakPtr()));
  for (auto& task : pending_tasks_) {
    std::move(task).Run();
  }
  pending_tasks_.clear();
  observers_.Notify(
      &PasswordReuseManager::Observer::HashPasswordManagerAvailable,
      hash_password_manager_.get());
}

void PasswordReuseManagerImpl::SchedulePasswordHashUpdate(
    std::optional<metrics_util::SignInState> sign_in_state_for_metrics) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  CHECK(hash_password_manager_);
  if (!reuse_detector_) {
    return;
  }

  std::vector<PasswordHashData> protected_password_data_list =
      hash_password_manager_->RetrieveAllPasswordHashes();

  std::vector<PasswordHashData> gaia_password_hash_list;
  std::vector<PasswordHashData> enterprise_password_hash_list;
  for (PasswordHashData& password_hash : protected_password_data_list) {
    if (password_hash.is_gaia_password) {
      gaia_password_hash_list.push_back(std::move(password_hash));
    } else {
      enterprise_password_hash_list.push_back(std::move(password_hash));
    }
  }

  if (sign_in_state_for_metrics) {
    metrics_util::LogProtectedPasswordHashCounts(gaia_password_hash_list.size(),
                                                 *sign_in_state_for_metrics);
  }

  ScheduleTask(base::BindOnce(&PasswordReuseDetector::UseGaiaPasswordHash,
                              base::Unretained(reuse_detector_.get()),
                              std::move(gaia_password_hash_list)));

  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::UseNonGaiaEnterprisePasswordHash,
                     base::Unretained(reuse_detector_.get()),
                     std::move(enterprise_password_hash_list)));
}

void PasswordReuseManagerImpl::ScheduleEnterprisePasswordURLUpdate() {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (DelayUntilReady(
          &PasswordReuseManagerImpl::ScheduleEnterprisePasswordURLUpdate)) {
    return;
  }
  if (!prefs_) {
    return;
  }
  std::vector<GURL> enterprise_login_urls;
  GURL enterprise_change_password_url;
  if (!reuse_detector_) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::UseEnterprisePasswordURLs,
                              base::Unretained(reuse_detector_.get()),
                              std::move(enterprise_login_urls),
                              std::move(enterprise_change_password_url)));
}

void PasswordReuseManagerImpl::RequestLoginsFromStores() {
  profile_store_observation_.Observe(profile_store_.get());
  profile_store_->GetAutofillableLogins(
      /*consumer=*/weak_ptr_factory_.GetWeakPtr());
  if (account_store_) {
    account_store_observation_.Observe(account_store_.get());
    account_store_->GetAutofillableLogins(
        /*consumer=*/weak_ptr_factory_.GetWeakPtr());
    // base::Unretained() is safe because `this` outlives the subscription.
    account_store_cb_list_subscription_ =
        account_store_->AddSyncEnabledOrDisabledCallback(base::BindRepeating(
            &PasswordReuseManagerImpl::AccountStoreStateChanged,
            base::Unretained(this)));
  }
}

void PasswordReuseManagerImpl::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  DCHECK(main_task_runner_->RunsTasksInCurrentSequence());
  if (std::holds_alternative<PasswordStoreBackendError>(results_or_error)) {
    return;
  }

  if (!hash_password_manager_) {
    CHECK(DelayUntilReady(
        &PasswordReuseManagerImpl::OnGetPasswordStoreResultsOrErrorFrom,
        base::Unretained(store), std::move(results_or_error)));
    return;
  }

  if (!reuse_detector_) {
    return;
  }
  auto results = std::get<LoginsResult>(std::move(results_or_error));
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnGetPasswordStoreResults,
                              base::Unretained(reuse_detector_.get()),
                              std::move(results)));
}

void PasswordReuseManagerImpl::OnLoginsChanged(
    password_manager::PasswordStoreInterface* store,
    const password_manager::PasswordStoreChangeList& changes) {
  if (DelayUntilReady(&PasswordReuseManagerImpl::OnLoginsChanged, nullptr,
                      changes)) {
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnLoginsChanged,
                              base::Unretained(reuse_detector_.get()),
                              changes));
}

void PasswordReuseManagerImpl::OnLoginsRetained(
    PasswordStoreInterface* store,
    const std::vector<StoredCredential>& retained_credentials) {
  PasswordForm::Store store_type = store == account_store_
                                       ? PasswordForm::Store::kAccountStore
                                       : PasswordForm::Store::kProfileStore;
  OnLoginsRetainedImpl(store_type, retained_credentials);
}

void PasswordReuseManagerImpl::OnLoginsRetainedImpl(
    PasswordForm::Store store_type,
    const std::vector<StoredCredential>& retained_credentials) {
  std::vector<StoredCredential> cloned_passwords;
  for (const auto& cred : retained_credentials) {
    cloned_passwords.push_back(CloneStoredCredential(cred));
  }

  if (!hash_password_manager_) {
    pending_tasks_.push_back(base::BindOnce(
        &PasswordReuseManagerImpl::OnLoginsRetainedImpl, base::Unretained(this),
        store_type, std::move(cloned_passwords)));
    return;
  }
  ScheduleTask(base::BindOnce(&PasswordReuseDetector::OnLoginsRetained,
                              base::Unretained(reuse_detector_.get()),
                              store_type, std::move(cloned_passwords)));
}

bool PasswordReuseManagerImpl::ScheduleTask(base::OnceClosure task) {
  return background_task_runner_ &&
         background_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void PasswordReuseManagerImpl::AccountStoreStateChanged() {
  DCHECK(account_store_);
  ScheduleTask(
      base::BindOnce(&PasswordReuseDetector::ClearCachedAccountStorePasswords,
                     base::Unretained(reuse_detector_.get())));
  account_store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordReuseManagerImpl::HashPasswordManagerStateChanged(
    const std::string& username) {
  observers_.Notify(
      &PasswordReuseManager::Observer::HashPasswordStateMaybeChanged, username,
      hash_password_manager_.get());
}

void PasswordReuseManagerImpl::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (DelayUntilReady(&PasswordReuseManagerImpl::OnPrimaryAccountChanged,
                      event_details)) {
    return;
  }
  if (!shared_pref_delegate_) {
    return;
  }
}

void PasswordReuseManagerImpl::MaybeSavePasswordHash(
    const PasswordForm* submitted_form,
    PasswordManagerClient* client,
    std::optional<metrics_util::GaiaPasswordHashChange> event) {
  // This method doesn't use DelayUntilReady since it isn't safe to store
  // `submitted_form` or `client` in a task. That's okay since this method
  // doesn't (and should never) use any member variables. It does call into
  // SaveEnterprisePasswordHash or SaveGaiaPasswordHash which are themselves
  // delayed if necessary.

  // When |username_value| is empty, it's not clear whether the submitted
  // credentials are really Gaia or enterprise credentials. Don't save
  // password hash in that case.
  std::string username = base::UTF16ToUTF8(submitted_form->username_value);
  if (username.empty()) {
    return;
  }

  bool should_save_enterprise_pw =
      client->GetStoreResultFilter()->ShouldSaveEnterprisePasswordHash(
          *submitted_form);
  bool should_save_gaia_pw =
      client->GetStoreResultFilter()->ShouldSaveGaiaPasswordHash(
          *submitted_form);

  if (!should_save_enterprise_pw && !should_save_gaia_pw) {
    return;
  }

  if (password_manager_util::IsLoggingActive(client)) {
    BrowserSavePasswordProgressLogger logger(client->GetCurrentLogManager());
    logger.LogMessage(
        autofill::SavePasswordProgressLogger::STRING_SAVE_PASSWORD_HASH);
  }

  // Canonicalizes username if it is an email.
  if (username.find('@') != std::string::npos) {
    username = gaia::CanonicalizeEmail(username);
  }
  bool is_password_change = !submitted_form->new_password_element.empty();
  const std::u16string password = is_password_change
                                      ? submitted_form->new_password_value
                                      : submitted_form->password_value;

  if (should_save_enterprise_pw) {
    SaveEnterprisePasswordHash(username, password);
    return;
  }

  CHECK(should_save_gaia_pw);
  bool is_sync_account_email =
      client->GetStoreResultFilter()->IsSyncAccountEmail(username);
  metrics_util::GaiaPasswordHashChange gaia_event = event.value_or(
      is_sync_account_email
          ? (is_password_change
                 ? metrics_util::GaiaPasswordHashChange::CHANGED_IN_CONTENT_AREA
                 : metrics_util::GaiaPasswordHashChange::SAVED_IN_CONTENT_AREA)
          : (is_password_change ? metrics_util::GaiaPasswordHashChange::
                                      NOT_SYNC_PASSWORD_CHANGE
                                : metrics_util::GaiaPasswordHashChange::
                                      SAVED_IN_CONTENT_AREA));
  SaveGaiaPasswordHash(username, password,
                       /*is_sync_password_for_metrics=*/is_sync_account_email,
                       gaia_event);
}

HashPasswordManager* PasswordReuseManagerImpl::GetHashPasswordManager() {
  return hash_password_manager_.get();
}

void PasswordReuseManagerImpl::AddObserver(
    PasswordReuseManager::Observer* observer) {
  CHECK(main_task_runner_->RunsTasksInCurrentSequence());
  observers_.AddObserver(observer);
}

void PasswordReuseManagerImpl::RemoveObserver(
    PasswordReuseManager::Observer* observer) {
  CHECK(main_task_runner_->RunsTasksInCurrentSequence());
  observers_.RemoveObserver(observer);
}

}  // namespace password_manager
