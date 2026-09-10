// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/flat_set.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/files/scoped_temp_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/json/json_file_value_serializer.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/enterprise_companion/device_management_storage/dm_storage.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/enterprise_companion/installer_paths.h"
#include "chrome/enterprise_companion/telemetry_logger/proto/log_request.pb.h"
#include "chrome/updater/branded_constants.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/ipc/ipc_support.h"
#include "chrome/updater/ping_configurator.h"
#include "chrome/updater/policy/dm_policy_manager.h"
#include "chrome/updater/protos/omaha_settings.pb.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/test/dm_policy_builder.h"
#include "chrome/updater/test/http_request.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/request_matcher.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test/test_scope.h"
#include "chrome/updater/test/unit_test_util.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_version.h"
#include "chrome/updater/util/util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/update_client/protocol_definition.h"
#include "components/update_client/update_client.h"
#include "components/update_client/utils.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include <unistd.h>

#include "base/environment.h"
#include "chrome/updater/util/posix_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/updater/test/integration_tests_mac.h"
#include "chrome/updater/util/mac_util.h"
#endif

namespace updater::test {
namespace {

namespace enterprise_management =
    ::wireless_android_enterprise_devicemanagement;
using enterprise_management::ApplicationSettings;
using enterprise_management::OmahaSettingsClientProto;

void ExpectNoUpdateSequence(
    ScopedServer& test_server,
    const std::string& app_id,
    const base::Version& updater_version = base::Version(kUpdaterVersion),
    std::optional<base::Version> app_version = std::nullopt) {
  base::DictValue app_expectation = base::DictValue().Set("appid", app_id);
  if (app_version) {
    app_expectation.Set("version", app_version->GetString());
  }
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(updater_version),
       request::GetJSONContentMatcher(base::DictValue().SetByDottedPath(
           "request.apps",
           base::ListValue().Append(std::move(app_expectation))))},
      base::BindRepeating(
          [](const std::string& app_id, bool v4) {
            return v4 ? absl::StrFormat(
                            ")]}'\n"
                            R"({"response":{)"
                            R"(  "protocol":"4.0",)"
                            R"(  "apps":[)"
                            R"(    {)"
                            R"(      "appid":"%s",)"
                            R"(      "status":"ok",)"
                            R"(      "updatecheck":{)"
                            R"(        "status":"noupdate")"
                            R"(      })"
                            R"(    })"
                            R"(  ])"
                            R"(}})",
                            app_id)
                      : absl::StrFormat(
                            ")]}'\n"
                            R"({"response":{)"
                            R"(  "protocol":"3.1",)"
                            R"(  "app":[)"
                            R"(    {)"
                            R"(      "appid":"%s",)"
                            R"(      "status":"ok",)"
                            R"(      "updatecheck":{)"
                            R"(        "status":"noupdate")"
                            R"(      })"
                            R"(    })"
                            R"(  ])"
                            R"(}})",
                            app_id);
          },
          app_id));
}

void ExpectPingRequest(
    ScopedServer& test_server,
    const std::string& app_id,
    const update_client::UpdateClient::PingParams& ping_params,
    const base::Version& version = base::Version(kUpdaterVersion)) {
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(version),
       request::GetContentMatcher({absl::StrFormat(
           R"(.*"appid":"%s".*"errorcode":%d,"eventresult":%d,"eventtype":%d,)"
           R"(%s.*)",
           app_id.c_str(), ping_params.error_code, ping_params.result,
           ping_params.event_type,
           ping_params.extra_code1
               ? absl::StrFormat(R"("extracode1":%d,)", ping_params.extra_code1)
                     .c_str()
               : "")})},
      absl::StrFormat(")]}'\n"
                      R"({"response":{)"
                      R"(  "protocol":"4.0",)"
                      R"(  "apps":[)"
                      R"(    {)"
                      R"(      "appid":"%s",)"
                      R"(      "status":"ok")"
                      R"(    })"
                      R"(  ])"
                      R"(}})",
                      app_id.c_str()));
}

void ExpectInstallEvent(
    ScopedServer& test_server,
    const std::string& app_id,
    std::optional<int> event_result = std::nullopt,
    std::optional<std::string> prev_version = std::nullopt) {
  base::DictValue event_expectation = base::DictValue().Set("eventtype", 2);
  if (event_result) {
    event_expectation.Set("eventresult", *event_result);
  }
  if (prev_version) {
    event_expectation.Set("previousversion", *prev_version);
  }
  base::DictValue app_expectation =
      base::DictValue()
          .Set("appid", app_id)
          .Set("events",
               base::ListValue().Append(std::move(event_expectation)));
  test_server.ExpectOnce(
      {request::GetJSONContentMatcher(base::DictValue().SetByDottedPath(
          "request.apps",
          base::ListValue().Append(std::move(app_expectation))))},
      absl::StrFormat(")]}'\n"
                      R"({"response":{)"
                      R"(  "protocol":"3.1",)"
                      R"(  "app":[)"
                      R"(    {)"
                      R"(      "appid":"%s",)"
                      R"(      "status":"ok")"
                      R"(    })"
                      R"(  ])"
                      R"(}})",
                      app_id));
}

// Expects that the file at `path` is readable by other users.
void ExpectUserReadable(const base::FilePath& path) {
#if BUILDFLAG(IS_POSIX)
  int mode = 0;
  ASSERT_TRUE(base::GetPosixFilePermissions(path, &mode));
  EXPECT_TRUE(mode & base::FILE_PERMISSION_READ_BY_OTHERS);
#else
  std::vector<base::win::Sid> sids;
  sids.push_back(base::win::Sid(base::win::WellKnownSid::kBuiltinUsers));
  EXPECT_TRUE(base::win::HasAccessToPath(path, sids, FILE_GENERIC_READ,
                                         /*inheritance=*/0));
#endif
}

base::FilePath GetInstallerPath(const std::string& installer) {
  return base::FilePath::FromUTF8Unsafe("test_installer").AppendUTF8(installer);
}

struct TestApp {
  std::string appid;
  base::Version v1;
  std::string v1_crx;
  base::Version v2;
  std::string v2_crx;

  base::CommandLine GetInstallCommandSwitches(bool install_v1) const {
    base::CommandLine command(base::CommandLine::NO_PROGRAM);
    if (IsSystemInstall(GetUpdaterScopeForTesting())) {
      command.AppendArg("--system");
    }
    command.AppendSwitchUTF8("--appid", appid);
    command.AppendSwitchUTF8("--company", COMPANY_SHORTNAME_STRING);
    command.AppendSwitchUTF8("--product_version",
                             install_v1 ? v1.GetString() : v2.GetString());
    return command;
  }

  std::string GetInstallCommandLineArgs(bool install_v1) const {
    return update_client::StringTypeToUTF8(
        GetInstallCommandSwitches(install_v1).GetCommandLineString());
  }

  base::CommandLine GetInstallCommandLine(bool install_v1) const {
    base::FilePath exe_path;
    base::PathService::Get(base::DIR_EXE, &exe_path);
    const base::FilePath installer_path =
        GetInstallerPath(install_v1 ? v1_crx : v2_crx);
    base::CommandLine command = GetInstallCommandSwitches(install_v1);
    command.SetProgram(exe_path.Append(
        installer_path.DirName().AppendUTF8("test_app_setup.sh")));
    return command;
  }
};

base::ListValue SetToList(const base::flat_set<std::string>& set) {
  base::ListValue list;
  for (const auto& elem : set) {
    list.Append(elem);
  }
  return list;
}

}  // namespace

class IntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    logging::SetLogItems(/*enable_process_id=*/true,
                         /*enable_thread_id=*/true,
                         /*enable_timestamp=*/true,
                         /*enable_tickcount=*/false);
    VLOG(2) << __func__ << " entered.";

    ASSERT_NO_FATAL_FAILURE(CleanProcesses());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(Clean());
    ASSERT_NO_FATAL_FAILURE(ExpectClean());
    ASSERT_NO_FATAL_FAILURE(EnterTestMode(
        GURL("http://localhost:1234"), GURL("http://localhost:1235"),
        /*app_logo_url=*/{}, /*event_logging_url=*/{}, base::Minutes(5)));
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(false));
#if BUILDFLAG(IS_LINUX)
    // On LUCI the XDG_RUNTIME_DIR and DBUS_SESSION_BUS_ADDRESS environment
    // variables may not be set. These are required for systemctl to connect to
    // its bus in user mode.
    std::unique_ptr<base::Environment> env = base::Environment::Create();
    const std::string xdg_runtime_dir =
        base::StrCat({"/run/user/", base::NumberToString(getuid())});
    if (!env->HasVar("XDG_RUNTIME_DIR")) {
      ASSERT_TRUE(env->SetVar("XDG_RUNTIME_DIR", xdg_runtime_dir));
    }
    if (!env->HasVar("DBUS_SESSION_BUS_ADDRESS")) {
      ASSERT_TRUE(
          env->SetVar("DBUS_SESSION_BUS_ADDRESS",
                      base::StrCat({"unix:path=", xdg_runtime_dir, "/bus"})));
    }
#endif

    // Mark the device as de-registered. This stops sending DM requests
    // that mess up the request expectations in the mock server.
    ASSERT_NO_FATAL_FAILURE(DMDeregisterDevice());

    // Abandon the test if there are any non-fatal errors during setup.
    ASSERT_FALSE(HasFailure());

    VLOG(2) << __func__ << "completed.";
  }

  void TearDown() override {
    VLOG(2) << __func__ << " entered.";
    if (IsSkipped()) {
      return;
    }

    ExitTestMode();
    if (!HasFailure()) {
      ExpectClean();
    }
    ExpectNoCrashes();

    PrintLog();
    CopyLog();

    DMCleanup();

    // Updater process must not be running for `Clean()` to succeed.
    ASSERT_TRUE(WaitForUpdaterExit());
    Clean();

    VLOG(2) << __func__ << "completed.";
  }

  void ExpectNoCrashes() { test_commands_->ExpectNoCrashes(); }

  void CopyLog() { test_commands_->CopyLog(/*infix=*/""); }

  void PrintLog() { test_commands_->PrintLog(); }

  void Install(const base::flat_set<std::string>& switches = {}) {
    test_commands_->Install(SetToList(switches));
  }

  void InstallUpdaterAndApp(
      const std::string& app_id,
      const bool is_silent_install,
      const std::string& tag,
      const std::string& child_window_text_to_find = {},
      const bool always_launch_cmd = false,
      const bool verify_app_logo_loaded = false,
      const bool expect_success = true,
      const bool wait_for_the_installer = true,
      const int expected_exit_code = 0,
      const base::flat_set<std::string>& additional_switches = {},
      const base::FilePath& updater_path = GetSetupExecutablePath()) {
    test_commands_->InstallUpdaterAndApp(
        app_id, is_silent_install, tag, child_window_text_to_find,
        always_launch_cmd, verify_app_logo_loaded, expect_success,
        wait_for_the_installer, expected_exit_code,
        SetToList(additional_switches), updater_path);
  }

  void ExpectInstalled() { test_commands_->ExpectInstalled(); }

  void Uninstall() {
    ASSERT_TRUE(WaitForUpdaterExit());
    ExpectNoCrashes();
    PrintLog();
    CopyLog();
    test_commands_->Uninstall();
    ASSERT_TRUE(WaitForUpdaterExit());
  }

  void ExpectCandidateUninstalled() {
    test_commands_->ExpectCandidateUninstalled();
  }

  void Clean() { test_commands_->Clean(); }

  void ExpectClean() { test_commands_->ExpectClean(); }

  void EnterTestMode(
      const GURL& update_url,
      const GURL& crash_upload_url,
      const GURL& app_logo_url,
      const GURL& event_logging_url,
      base::TimeDelta idle_timeout,
      base::TimeDelta server_keep_alive_time = base::Seconds(2),
      base::TimeDelta ceca_connection_timeout = base::Seconds(10),
      std::optional<EventLoggingPermissionProvider>
          event_logging_permission_provider = std::nullopt) {
    test_commands_->EnterTestMode(
        update_url, crash_upload_url, app_logo_url, event_logging_url,
        idle_timeout, server_keep_alive_time, ceca_connection_timeout,
        event_logging_permission_provider);
  }

  void ExitTestMode() { test_commands_->ExitTestMode(); }

  void SetDictPolicies(const base::DictValue& values) {
    test_commands_->SetDictPolicies(values);
  }

  void SetPlatformPolicies(const base::DictValue& values) {
    test_commands_->SetPlatformPolicies(values);
  }

  void SetMachineManaged(bool is_managed_device) {
    test_commands_->SetMachineManaged(is_managed_device);
  }

  void ExpectVersionActive(const std::string& version) {
    test_commands_->ExpectVersionActive(version);
  }

  void ExpectVersionNotActive(const std::string& version) {
    test_commands_->ExpectVersionNotActive(version);
  }

  void InstallAppViaService(const std::string& app_id,
                            const base::DictValue& expected_final_values = {}) {
    test_commands_->InstallAppViaService(app_id, expected_final_values);
  }

  void SetupFakeUpdaterHigherVersion() {
    test_commands_->SetupFakeUpdaterHigherVersion();
  }

  void SetupFakeUpdaterLowerVersion() {
    test_commands_->SetupFakeUpdaterLowerVersion();
  }

  void SetupRealUpdater(const base::FilePath& updater_path,
                        const base::flat_set<std::string>& switches = {}) {
    test_commands_->SetupRealUpdater(updater_path, SetToList(switches));
  }

  void SetActive(const std::string& app_id) {
    test_commands_->SetActive(app_id);
  }

  void ExpectActive(const std::string& app_id) {
    test_commands_->ExpectActive(app_id);
  }

  void ExpectNotActive(const std::string& app_id) {
    test_commands_->ExpectNotActive(app_id);
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) {
    test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void SetServerStarts(int value) { test_commands_->SetServerStarts(value); }

  void FillLog() { test_commands_->FillLog(); }

  void ExpectLogRotated() { test_commands_->ExpectLogRotated(); }

  void ExpectRegistered(const std::string& app_id) {
    test_commands_->ExpectRegistered(app_id);
  }

  void ExpectNotRegistered(const std::string& app_id) {
    test_commands_->ExpectNotRegistered(app_id);
  }

  void ExpectAppTag(const std::string& app_id, const std::string& tag) {
    test_commands_->ExpectAppTag(app_id, tag);
  }

  void SetAppTag(const std::string& app_id, const std::string& tag) {
    test_commands_->SetAppTag(app_id, tag);
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) {
    test_commands_->ExpectAppVersion(app_id, version);
  }

  void InstallApp(const std::string& app_id,
                  const base::Version& version = base::Version("0.1")) {
    test_commands_->InstallApp(app_id, version);
  }

  void UninstallApp(const std::string& app_id) {
    test_commands_->UninstallApp(app_id);
  }

  void RunWake(int exit_code,
               const base::Version& version = base::Version(kUpdaterVersion)) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWake(exit_code, version);
  }

  void RunWakeAll() {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeAll();
  }

  void RunCrashMe() { test_commands_->RunCrashMe(); }

  void RunWakeActive(int exit_code) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunWakeActive(exit_code);
  }

  void RunServer(int exit_code, bool internal) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunServer(exit_code, internal);
  }

  void RunUpdateApps(
      int exit_code,
      const base::Version& version = base::Version(kUpdaterVersion)) {
    ASSERT_TRUE(WaitForUpdaterExit());
    test_commands_->RunUpdateApps(exit_code, version);
  }

  void CheckForUpdate(const std::string& app_id) {
    test_commands_->CheckForUpdate(app_id);
  }

  void ExpectCheckForUpdateOppositeScopeFails(const std::string& app_id) {
    test_commands_->ExpectCheckForUpdateOppositeScopeFails(app_id);
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) {
    test_commands_->Update(app_id, install_data_index);
  }

  void UpdateAll() { test_commands_->UpdateAll(); }

  void GetAppStates(const base::DictValue& expected_app_states) {
    test_commands_->GetAppStates(expected_app_states);
  }

  void DeleteUpdaterDirectory() { test_commands_->DeleteUpdaterDirectory(); }

  void DeleteActiveUpdaterExecutable() {
    test_commands_->DeleteActiveUpdaterExecutable();
  }

  void DeleteFile(const base::FilePath& path) {
    test_commands_->DeleteFile(path);
  }

  base::FilePath GetDifferentUserPath() {
    return test_commands_->GetDifferentUserPath();
  }

  void ExpectUpdateCheckRequest(ScopedServer& test_server) {
    test_commands_->ExpectUpdateCheckRequest(test_server);
  }

  void ExpectUpdateCheckSequence(
      ScopedServer& test_server,
      const std::string& app_id,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version,
      const base::Version& updater_version = base::Version(kUpdaterVersion)) {
    test_commands_->ExpectUpdateCheckSequence(test_server, app_id, priority,
                                              from_version, to_version,
                                              updater_version);
  }

  void ExpectUninstallPing(ScopedServer& test_server,
                           std::optional<GURL> target_url = {}) {
    test_commands_->ExpectPing(test_server,
                               update_client::protocol_request::kEventUninstall,
                               target_url);
  }

  void ExpectInstallSource(ScopedServer& test_server,
                           const std::string& install_source) {
    test_commands_->ExpectInstallSource(test_server, install_source);
  }

  void ExpectAppCommandPing(
      ScopedServer& test_server,
      const std::string& appid,
      const std::string& appcommandid,
      int errorcode,
      int eventresult,
      int event_type,
      const base::Version& version,
      const base::Version& updater_version = base::Version(kUpdaterVersion)) {
    test_commands_->ExpectAppCommandPing(test_server, appid, appcommandid,
                                         errorcode, eventresult, event_type,
                                         version, updater_version);
  }

  void ExpectUpdateSequence(
      ScopedServer& test_server,
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version,
      bool do_fault_injection = false,
      bool skip_download = false,
      const base::Version& updater_version = base::Version(kUpdaterVersion),
      const std::string& event_regex = ".*",
      bool use_xz = false) {
    test_commands_->ExpectUpdateSequence(
        test_server, app_id, install_data_index, priority, from_version,
        to_version, do_fault_injection, skip_download, updater_version,
        event_regex, use_xz);
  }

  void ExpectUpdateSequenceBadHash(ScopedServer& test_server,
                                   const std::string& app_id,
                                   const std::string& install_data_index,
                                   UpdateService::Priority priority,
                                   const base::Version& from_version,
                                   const base::Version& to_version) {
    test_commands_->ExpectUpdateSequenceBadHash(test_server, app_id,
                                                install_data_index, priority,
                                                from_version, to_version);
  }

  void ExpectSelfUpdateSequence(ScopedServer& test_server) {
    test_commands_->ExpectSelfUpdateSequence(test_server);
  }

  void ExpectInstallSequence(
      ScopedServer& test_server,
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version,
      bool do_fault_injection = false,
      bool skip_download = false,
      const base::Version& updater_version = base::Version(kUpdaterVersion),
      const std::string& event_regex = ".*") {
    test_commands_->ExpectInstallSequence(
        test_server, app_id, install_data_index, priority, from_version,
        to_version, do_fault_injection, skip_download, updater_version,
        event_regex);
  }

  void StressUpdateService() { test_commands_->StressUpdateService(); }

  void CallServiceUpdate(
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::PolicySameVersionUpdate policy_same_version_update) {
    test_commands_->CallServiceUpdate(app_id, install_data_index,
                                      policy_same_version_update);
  }

  void SetupFakeLegacyUpdater() { test_commands_->SetupFakeLegacyUpdater(); }

#if BUILDFLAG(IS_MAC)
  void PrivilegedHelperInstall() { test_commands_->PrivilegedHelperInstall(); }
  void DeleteLegacyUpdater() { test_commands_->DeleteLegacyUpdater(); }
  void ExpectPrepareToRunBundleSuccess(const base::FilePath& bundle_path) {
    test_commands_->ExpectPrepareToRunBundleSuccess(bundle_path);
  }

  void ExpectKSAdminFetchTag(bool elevate,
                             const std::string& product_id,
                             const base::FilePath& xc_path,
                             std::optional<UpdaterScope> store_flag,
                             std::optional<std::string> want_tag) {
    test_commands_->ExpectKSAdminFetchTag(elevate, product_id, xc_path,
                                          store_flag, want_tag);
  }

  void ExpectKSAdminXattrBrand(bool elevate,
                               const base::FilePath& path,
                               std::optional<std::string> want_brand) {
    test_commands_->ExpectKSAdminXattrBrand(elevate, path,
                                            std::move(want_brand));
  }

  void ExpectCRURegistrationChecksForUpdate(
      const std::string& app_id,
      const base::FilePath& xc_path,
      const std::string& expected_version) {
    test_commands_->ExpectCRURegistrationChecksForUpdate(app_id, xc_path,
                                                         expected_version);
  }

#endif  // BUILDFLAG(IS_MAC)

  void ExpectAppInstalled(const std::string& appid,
                          const base::Version& expected_version) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, expected_version));

    // Verify installed app artifacts.
    const base::FilePath app_json_path =
        GetInstallDirectory(GetUpdaterScopeForTesting())
            ->DirName()
            .AppendUTF8(appid)
            .AppendUTF8("app.json");
    JSONFileValueDeserializer parser(app_json_path,
                                     base::JSON_ALLOW_TRAILING_COMMAS);
    int error_code = 0;
    std::string error_message;
    std::unique_ptr<base::Value> app_data(
        parser.Deserialize(&error_code, &error_message));
    EXPECT_EQ(error_code, 0)
        << "Failed to load app json file at: " << app_json_path;
    EXPECT_TRUE(app_data);
    EXPECT_TRUE(app_data->is_dict());
    const base::DictValue& app_info = app_data->GetDict();
    EXPECT_EQ(*app_info.FindString("app"), appid);
    EXPECT_EQ(*app_info.FindString("company"), COMPANY_SHORTNAME_STRING);
    EXPECT_EQ(*app_info.FindString("pv"), expected_version.GetString());
  }

  void InstallTestApp(const TestApp& app, bool install_v1 = true) {
    const base::Version version = install_v1 ? app.v1 : app.v2;
    InstallApp(app.appid, version);
    base::FilePath exe_path;
    ASSERT_TRUE(base::PathService::Get(base::DIR_EXE, &exe_path));
    const base::CommandLine command = app.GetInstallCommandLine(install_v1);
    VLOG(2) << "Launch app setup command: " << command.GetCommandLineString();
    const base::Process process = base::LaunchProcess(
        IsSystemInstall(GetUpdaterScopeForTesting()) ? MakeElevated(command)
                                                     : command,
        {});
    if (!process.IsValid()) {
      VLOG(2) << "Failed to launch the app setup command.";
    }
    int exit_code = -1;
    EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_timeout(),
                                               &exit_code));
    EXPECT_EQ(0, exit_code);
    SetExistenceCheckerPath(app.appid,
                            GetInstallDirectory(GetUpdaterScopeForTesting())
                                ->DirName()
                                .AppendUTF8(app.appid));

    ExpectAppInstalled(app.appid, version);
  }

  void ExpectLegacyUpdaterMigrated() {
    test_commands_->ExpectLegacyUpdaterMigrated();
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) {
    test_commands_->RunRecoveryComponent(app_id, version);
  }

  void SetLastChecked(base::Time time) { test_commands_->SetLastChecked(time); }

  void ExpectLastChecked() { test_commands_->ExpectLastChecked(); }

  void ExpectLastStarted() { test_commands_->ExpectLastStarted(); }

  void RunOfflineInstall(bool is_legacy_install,
                         bool is_silent_install,
                         int installer_result = 0,
                         int installer_error = 0,
                         const std::string& install_source = "") {
    test_commands_->RunOfflineInstall(is_legacy_install, is_silent_install,
                                      installer_result, installer_error,
                                      install_source);
  }

  void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                       bool is_silent_install,
                                       const std::string& language = "en") {
    test_commands_->RunOfflineInstallOsNotSupported(
        is_legacy_install, is_silent_install, language);
  }

  void RunMockOfflineMetaInstall(const std::string& app_id,
                                 const base::Version& version,
                                 const std::string& tag,
                                 const base::FilePath& installer_path,
                                 const std::string& arguments,
                                 bool is_silent_install,
                                 const std::string& platform,
                                 const std::string& installer_text,
                                 const bool always_launch_cmd,
                                 const int expected_exit_code,
                                 bool expect_success) {
    test_commands_->RunMockOfflineMetaInstall(
        app_id, version, tag, installer_path, arguments, is_silent_install,
        platform, installer_text, always_launch_cmd, expected_exit_code,
        expect_success);
  }

  void DMPushEnrollmentToken(const std::string& enrollment_token) {
    test_commands_->DMPushEnrollmentToken(enrollment_token);
  }

  void DMDeregisterDevice() { test_commands_->DMDeregisterDevice(); }

  void DMCleanup() { test_commands_->DMCleanup(); }

  void InstallEnterpriseCompanionApp() {
    test_commands_->InstallEnterpriseCompanionApp();
  }

  void InstallEnterpriseCompanionAppOverrides(
      const base::DictValue& external_overrides) {
    test_commands_->InstallEnterpriseCompanionAppOverrides(external_overrides);
  }

  void ExpectEnterpriseCompanionAppNotInstalled() {
    test_commands_->ExpectEnterpriseCompanionAppNotInstalled();
  }

  void UninstallEnterpriseCompanionApp() {
    test_commands_->UninstallEnterpriseCompanionApp();
  }

  scoped_refptr<IntegrationTestCommands> test_commands_ =
      CreateIntegrationTestCommands();

  static constexpr char kGlobalPolicyKey[] = "global";
  const TestApp kApp1 = {
      "test1", base::Version("1.0.0.0"), "test_installer_test1_v1.crx3",
      base::Version("2.0.0.0"), "test_installer_test1_v2.crx3"};
  const TestApp kApp2 = {
      "test2", base::Version("100.0.0.0"), "test_installer_test2_v1.crx3",
      base::Version("101.0.0.0"), "test_installer_test2_v2.crx3"};
  const TestApp kApp3 = {"test3", base::Version("1.0"),
                         "test_installer_test3_v1.crx3", base::Version("1.1"),
                         "test_installer_test3_v2.crx3"};

 private:
  base::test::TaskEnvironment environment_;
  ScopedIPCSupportWrapper ipc_support_;
};

#if defined(ADDRESS_SANITIZER)
#define MAYBE_UpdateServiceStress DISABLED_UpdateServiceStress
#else
#define MAYBE_UpdateServiceStress UpdateServiceStress
#endif

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTest, DoNothing) {}

TEST_F(IntegrationTest, Install) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests running the installer when the updater is already installed at the
// same version. It should have no notable effect.
TEST_F(IntegrationTest, OverinstallRedundant) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationLowerVersionTest
    : public ::testing::WithParamInterface<TestUpdaterVersion>,
      public IntegrationTest {};

INSTANTIATE_TEST_SUITE_P(IntegrationLowerVersionTestCases,
                         IntegrationLowerVersionTest,
                         ::testing::ValuesIn(GetRealUpdaterLowerVersions()));

TEST_P(IntegrationLowerVersionTest, OverinstallWorking) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // A new version hands off installation to the old version, and doesn't
  // change the active version of the updater.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // After two wakes, the new updater is active.
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_P(IntegrationLowerVersionTest, OverinstallBroken) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the old version is not working, the new version should install and
  // become active.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());

  // Cleanup the older version by reinstalling and uninstalling.
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, OverinstallBrokenSameVersion) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(DeleteActiveUpdaterExecutable());

  // Since the existing version is now not working, it should reinstall. This
  // will ultimately result in no visible change to the prefs file since the
  // new active version number will be the same as the old one.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUninstallOutdatedUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterHigherVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectCandidateUninstalled());
  // The candidate uninstall should not have altered global prefs.
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive("0.0.0.0"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  // Do not call `Uninstall()` since the outdated updater uninstalled itself.
  // Additional clean up is needed because of how this test is set up. After
  // the outdated instance uninstalls, a few files are left in the product
  // directory: prefs.json, updater.log, and overrides.json. These files are
  // owned by the active instance of the updater but in this case there is
  // no active instance left; therefore, explicit clean up is required.
  PrintLog();
  CopyLog();
  Clean();
}

TEST_F(IntegrationTest, QualifyUpdater) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  ExpectInstallEvent(test_server, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // This instance is now qualified and should activate itself and check itself
  // for updates on the next check.
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher({absl::StrFormat(".*%s.*", kUpdaterAppId)})},
      ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationCleanupOldVersionTest
    : public ::testing::WithParamInterface<TestUpdaterVersion>,
      public IntegrationTest {};

INSTANTIATE_TEST_SUITE_P(IntegrationCleanupOldVersionTestCases,
                         IntegrationCleanupOldVersionTest,
                         ::testing::ValuesIn(GetRealUpdaterVersions()));

TEST_P(IntegrationCleanupOldVersionTest, VariousArchitectures) {
  if (!GetParam().version.IsValid()) {
    GTEST_SKIP() << "Skipping test since the version for "
                 << GetParam().updater_setup_path << " is not valid";
  }

  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());

  // Since the old version is not working, the real version should install and
  // become active, even if the real version is a different architecture from
  // the native architecture.
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(GetParam().version.GetString()));

  // Waking the new version should clean up the old.
  ASSERT_NO_FATAL_FAILURE(RunWake(0, GetParam().version));
  ASSERT_TRUE(WaitForUpdaterExit());
  std::optional<base::FilePath> path =
      GetInstallDirectory(GetUpdaterScopeForTesting());
  ASSERT_TRUE(path);
  int dirs = 0;
  base::FileEnumerator(*path, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&dirs](const base::FilePath& path) {
        if (base::Version(path.BaseName().AsUTF8Unsafe()).IsValid()) {
          ++dirs;
        }
      });
  EXPECT_EQ(dirs, 1);

  // Cleanup by overinstalling the current version and uninstalling.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdate) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ScopedServer test_server(test_commands_);
  base::Version next_version(absl::StrFormat("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SelfUpdateWithWakeAll) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));

  base::Version next_version(absl::StrFormat("%s1", kUpdaterVersion));
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));

  ASSERT_NO_FATAL_FAILURE(RunWakeAll());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoSelfUpdateIfNoEula) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install({kEulaRequiredSwitch}));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(
      ExpectAppVersion(kUpdaterAppId, base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !BUILDFLAG(IS_LINUX)
// InstallAppViaService does not work on Linux.
TEST_F(IntegrationTest, SelfUpdateAfterEulaAcceptedViaInstall) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(Install({kEulaRequiredSwitch}));

  // Installing an app implies EULA accepted.
  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      GetUpdaterScopeForTesting(), test_server,
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));

  base::Version next_version(absl::StrFormat("%s1", kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kUpdaterAppId, "", UpdateService::Priority::kBackground,
      base::Version(kUpdaterVersion), next_version));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kUpdaterAppId, next_version));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !BUILDFLAG(IS_LINUX)

TEST_F(IntegrationTest, ReportsActive) {
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock.
  ASSERT_GE(TestTimeouts::action_timeout(), base::Seconds(18));
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());
  ScopedServer test_server(test_commands_);

  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Register apps test1 and test2. Expect pings for each.
  ExpectInstallEvent(test_server, "test1");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ExpectInstallEvent(test_server, "test2");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  // Set test1 to be active and do a background updatecheck.
  ASSERT_NO_FATAL_FAILURE(SetActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"(.*"appid":"test1","enabled":true,"installdate":-1,)",
            R"("ping":{"ad":-1,.*)"})},
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"4.0","daystart":{"elapsed_)"
      R"(days":5098}},"apps":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  // The updater has cleared the active bits.
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests calling `CheckForUpdate` when the updater is not installed.
TEST_F(IntegrationTest, CheckForUpdate_UpdaterNotInstalled) {
  scoped_refptr<UpdateService> update_service =
      CreateUpdateServiceProxy(GetUpdaterScopeForTesting());
  base::RunLoop loop;
  update_service->CheckForUpdate(
      "test", UpdateService::Priority::kForeground,
      UpdateService::PolicySameVersionUpdate::kNotAllowed,
      /*language=*/{}, base::DoNothing(),
      base::BindLambdaForTesting([&loop](UpdateService::Result result) {
        EXPECT_TRUE(result == UpdateService::Result::kServiceFailed ||
                    result == UpdateService::Result::kIPCConnectionFailed)
            << "result == " << result;
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(IntegrationTest, CheckForUpdate) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ExpectInstallEvent(test_server, kAppId);
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server, kAppId, UpdateService::Priority::kForeground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, GetUpdaterState) {
#if !BUILDFLAG(IS_MAC)
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterLowerVersion());
  ASSERT_NO_FATAL_FAILURE(SetupFakeUpdaterHigherVersion());
#endif  // !BUILDFLAG(IS_MAC)

  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Minutes(1)));
  ASSERT_NO_FATAL_FAILURE(Install());

  {
    scoped_refptr<UpdateService> update_service =
        CreateUpdateServiceProxy(GetUpdaterScopeForTesting());
    base::RunLoop loop;
    update_service->GetUpdaterState(base::BindLambdaForTesting(
        [&](const UpdateService::UpdaterState& result) {
          EXPECT_EQ(result.active_version, kUpdaterVersion);

#if !BUILDFLAG(IS_MAC)
          std::vector<uint32_t> components =
              base::Version(kUpdaterVersion).components();
          const base::CheckedNumeric<uint32_t> new_version = components[0] + 1;
          ASSERT_TRUE(new_version.AssignIfValid(&components[0]));

          EXPECT_THAT(result.inactive_versions,
                      testing::UnorderedElementsAre(
                          "100.0.0.0",
                          base::Version(std::move(components)).GetString()));
#endif  // !BUILDFLAG(IS_MAC)

          EXPECT_THAT(result.last_checked,
                      testing::AllOf(
                          testing::Lt(base::Time::Now()),
                          testing::Gt(base::Time::Now() - base::Minutes(2))));
          EXPECT_GE(base::Time::Now(), result.last_started);
          loop.Quit();
        }));
    loop.Run();
  }

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, GetPoliciesJson) {
  base::DictValue dict_policies;
  dict_policies.Set("autoupdatecheckperiodminutes",
                    base::Seconds(64800).InMinutes());
  dict_policies.Set("updatessuppressedstarthour", 15);
  dict_policies.Set("updatessuppressedstartmin", 0);
  dict_policies.Set("updatessuppresseddurationmin", 120);

  // Set app policy for `test1` to force install.
  dict_policies.Set("installtest1", 6);

  // Set app policy for `test2` to allow automatic updates only.
  dict_policies.Set("updatetest2", 3);
  ASSERT_NO_FATAL_FAILURE(SetDictPolicies(dict_policies));
  ASSERT_NO_FATAL_FAILURE(Install());

  {
    scoped_refptr<UpdateService> update_service =
        CreateUpdateServiceProxy(GetUpdaterScopeForTesting());
    base::RunLoop loop;
    update_service->GetPoliciesJson(
        base::BindLambdaForTesting([&](const std::string& result) {
          const auto root = base::JSONReader::Read(
              result, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
          ASSERT_TRUE(root);
          const base::DictValue& app_policies = CHECK_DEREF(
              CHECK_DEREF(root).GetDict().FindDict("policiesByAppId"));
          const base::DictValue& test1_install_policy = CHECK_DEREF(
              CHECK_DEREF(app_policies.FindDict("test1")).FindDict("Install"));
          EXPECT_EQ(
              CHECK_DEREF(test1_install_policy.FindString("prevailingSource")),
              "DictValuePolicy");
          EXPECT_EQ(CHECK_DEREF(CHECK_DEREF(test1_install_policy.FindDict(
                                                "valuesBySource"))
                                    .FindInt("DictValuePolicy")),
                    6);
          const base::DictValue& test2_update_policy = CHECK_DEREF(
              CHECK_DEREF(app_policies.FindDict("test2")).FindDict("Update"));
          EXPECT_EQ(
              CHECK_DEREF(test2_update_policy.FindString("prevailingSource")),
              "DictValuePolicy");
          EXPECT_EQ(CHECK_DEREF(CHECK_DEREF(test2_update_policy.FindDict(
                                                "valuesBySource"))
                                    .FindInt("DictValuePolicy")),
                    3);

          const base::DictValue& updater_policies = CHECK_DEREF(
              CHECK_DEREF(root).GetDict().FindDict("policiesByName"));
          const base::DictValue& last_check_period_policy =
              CHECK_DEREF(updater_policies.FindDict("LastCheckPeriod"));
          EXPECT_EQ(CHECK_DEREF(last_check_period_policy.FindString(
                        "prevailingSource")),
                    "DictValuePolicy");
          EXPECT_EQ(CHECK_DEREF(CHECK_DEREF(last_check_period_policy.FindDict(
                                                "valuesBySource"))
                                    .FindString("DictValuePolicy")),
                    "64800000000");
          const base::DictValue& updates_suppressed_policy =
              CHECK_DEREF(updater_policies.FindDict("UpdatesSuppressed"));
          EXPECT_EQ(CHECK_DEREF(updates_suppressed_policy.FindString(
                        "prevailingSource")),
                    "DictValuePolicy");
          EXPECT_EQ(CHECK_DEREF(CHECK_DEREF(updates_suppressed_policy.FindDict(
                                                "valuesBySource"))
                                    .FindDict("DictValuePolicy")),
                    base::DictValue()
                        .Set("Duration", 120)
                        .Set("StartHour", 15)
                        .Set("StartMinute", 0));
          loop.Quit();
        }));
    loop.Run();
  }

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateBadHash) {
  ASSERT_NO_FATAL_FAILURE(Install());
  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequenceBadHash(
      test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), base::Version("1")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateErrorStatus) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));

  ScopedServer test_server(test_commands_);
  for (const char* app_response_status :
       {"noupdate", "error-internal", "error-hash", "error-osnotsupported",
        "error-hwnotsupported", "error-unsupportedprotocol"}) {
    ExpectAppsUpdateSequence(
        GetUpdaterScopeForTesting(), test_server, {},
        {
            AppUpdateExpectation(
                kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
                kApp1.appid, kApp1.v1, kApp1.v2,
                /*is_install=*/false,
                /*should_update=*/false, false, "", "",
                GetInstallerPath(kApp1.v2_crx),
                /*always_serve_crx=*/false,
                /*error_category=*/UpdateService::ErrorCategory::kNone,
                /*error_code=*/0,
                /*event_type=*/0,
                /*custom_app_response=*/{}, app_response_status),
        });
    ASSERT_NO_FATAL_FAILURE(RunWake(0));
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1))
        << "App is unexpectedly updated with update check status: "
        << app_response_status;
    ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Hours(9)))
        << "Failed to set last-checked to force next update check.";
  }

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateApp) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  base::Version v2("2");
  const std::string kInstallDataIndex("test_install_data_index");
  // Skip the download in this case, because it is already in cache from the
  // previous update sequence. A real update would use a different CRX for v2.
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kAppId, kInstallDataIndex,
      UpdateService::Priority::kForeground, v1, v2, false, true));
  ASSERT_NO_FATAL_FAILURE(Update(kAppId, kInstallDataIndex));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));
  ASSERT_NO_FATAL_FAILURE(ExpectLastChecked());
  ASSERT_NO_FATAL_FAILURE(ExpectLastStarted());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateAppXZ) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kAppId, "", UpdateService::Priority::kBackground,
      base::Version("0.1"), v1, false, false, base::Version(kUpdaterVersion),
      ".*", true));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UpdateApps) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));
  base::Version v1("1");
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version("0.1"), v1));
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(
      test_server, kUpdaterAppId, base::Version(kUpdaterVersion),
      base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(RunUpdateApps(0));

  base::Version v2("2");
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground, v1, v2,
      false, true));
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(
      test_server, kUpdaterAppId, base::Version(kUpdaterVersion),
      base::Version(kUpdaterVersion)));
  ASSERT_NO_FATAL_FAILURE(RunUpdateApps(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v2));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SendPing) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }

  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const update_client::UpdateClient::PingParams ping_params{
      .event_type = update_client::protocol_request::kEventInstall,
      .result = 0,
      .error_code = 111,
      .extra_code1 = 222,
  };
  ASSERT_NO_FATAL_FAILURE(ExpectPingRequest(test_server, kAppId, ping_params));

  base::WaitableEvent ping_complete_event;
  base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives()})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](base::WaitableEvent& ping_complete_event,
                 const std::string& app_id,
                 const update_client::UpdateClient::PingParams& ping_params) {
                update_client::CrxComponent ping_data;
                ping_data.app_id = app_id;
                ping_data.requires_network_encryption = false;
                update_client::UpdateClientFactory(CreatePingConfigurator())
                    ->SendPing(ping_data, ping_params,
                               base::BindOnce(
                                   [](base::WaitableEvent& ping_complete_event,
                                      update_client::Error error) {
                                     ping_complete_event.Signal();
                                   },
                                   std::ref(ping_complete_event)));
              },
              std::ref(ping_complete_event), kAppId, ping_params));

  EXPECT_TRUE(ping_complete_event.TimedWait(TestTimeouts::action_timeout()));
}

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecently) {
  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Minutes(5)));
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(test_server, "test");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenLastCheckedRecentlyPolicy) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  base::DictValue dict_policies;
  dict_policies.Set("autoupdatecheckperiodminutes", 60 * 18);
  ASSERT_NO_FATAL_FAILURE(SetLastChecked(base::Time::Now() - base::Hours(12)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetDictPolicies(dict_policies));
  ExpectInstallEvent(test_server, "test");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, NoCheckWhenSuppressed) {
  ScopedServer test_server(test_commands_);
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);
  base::DictValue dict_policies;
  dict_policies.Set("updatessuppressedstarthour", (now.hour - 1 + 24) % 24);
  dict_policies.Set("updatessuppressedstartmin", 0);
  dict_policies.Set("updatessuppresseddurationmin", 120);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(SetDictPolicies(dict_policies));
  ExpectInstallEvent(test_server, "test");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndApp) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(
      InstallUpdaterAndApp(kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallUpdaterAndTwoApps) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const std::string kAppId2("test2");
  const base::Version v1("1");
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  // The download is skipped because the CRX was cached when installing the
  // first app.
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId2, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1, false, true));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId2, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId2, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId2, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId2, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ReferralId) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true, "referral=foobar&usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, ChangeTag) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo&usagestats=1"})));
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({1}), v1, false, true));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=foo2&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId, "foo2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SetTagRoundTrip) {
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(InstallApp("test"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag("test", ""));

  ASSERT_NO_FATAL_FAILURE(SetAppTag("test", "abc"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag("test", "abc"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(IS_MAC)
TEST_F(IntegrationTest, XattrTagWriteRead) {
  base::ScopedTempFile tag_me;
  ASSERT_TRUE(tag_me.Create());
  EXPECT_TRUE(tagging::WriteTagStringToApplicationInstanceXattr(
      tag_me.path(),
      "brand=TEST&iid=TestInstallId&appguid=org.chromium.test&ap=example"));

  base::expected<tagging::TagArgs, tagging::ErrorCode> read_result =
      tagging::ReadTagFromApplicationInstanceXattr(tag_me.path());
  ASSERT_TRUE(read_result.has_value())
      << "couldn't read tag: " << read_result.error();

  EXPECT_EQ(read_result->brand_code, "TEST");
  EXPECT_EQ(read_result->installation_id, "TestInstallId");

  ASSERT_EQ(read_result->apps.size(), 1u);
  const tagging::AppArgs& app_args = read_result->apps[0];
  EXPECT_EQ(app_args.app_id, "org.chromium.test");
  EXPECT_EQ(app_args.ap, "example");
}

TEST_F(IntegrationTest, NoTagXattrRead) {
  base::ScopedTempFile dont_tag_me;
  ASSERT_TRUE(dont_tag_me.Create());
  base::expected<tagging::TagArgs, tagging::ErrorCode> read_result =
      tagging::ReadTagFromApplicationInstanceXattr(dont_tag_me.path());
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), tagging::ErrorCode::kTagNotFound);
}

TEST_F(IntegrationTest, EmptyTagXattrRead) {
  base::ScopedTempFile tag_me;
  ASSERT_TRUE(tag_me.Create());
  EXPECT_TRUE(
      tagging::WriteTagStringToApplicationInstanceXattr(tag_me.path(), ""));

  base::expected<tagging::TagArgs, tagging::ErrorCode> read_result =
      tagging::ReadTagFromApplicationInstanceXattr(tag_me.path());

  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), tagging::ErrorCode::kTagNotFound);
}

TEST_F(IntegrationTest, NoXattrReadPath) {
  base::expected<tagging::TagArgs, tagging::ErrorCode> read_result =
      tagging::ReadTagFromApplicationInstanceXattr({});
  ASSERT_FALSE(read_result.has_value());
  EXPECT_EQ(read_result.error(), tagging::ErrorCode::kTagNotFound);
}

TEST_F(IntegrationTest, KSAdminXattrTagReadBrandSuccess) {
  ASSERT_NO_FATAL_FAILURE(Install());
  base::ScopedTempFile tag_me;
  ASSERT_TRUE(tag_me.Create());
  EXPECT_TRUE(tagging::WriteTagStringToApplicationInstanceXattr(
      tag_me.path(),
      "brand=TEST&iid=TestInstallId&appguid=org.chromium.test&ap=example"));
  ExpectKSAdminXattrBrand(false, tag_me.path(), "TEST");
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, KSAdminXattrTagReadNoBrandSuccess) {
  ASSERT_NO_FATAL_FAILURE(Install());
  base::ScopedTempFile tag_me_without_brand;
  ASSERT_TRUE(tag_me_without_brand.Create());
  EXPECT_TRUE(tagging::WriteTagStringToApplicationInstanceXattr(
      tag_me_without_brand.path(),
      "iid=TestInstallId&appguid=org.chromium.test&ap=example"));
  ExpectKSAdminXattrBrand(false, tag_me_without_brand.path(), "");
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, KSAdminXattrTagBrandNoXattrFailure) {
  ASSERT_NO_FATAL_FAILURE(Install());
  base::ScopedTempFile dont_tag_me;
  ASSERT_TRUE(dont_tag_me.Create());
  ExpectKSAdminXattrBrand(false, dont_tag_me.path(), std::nullopt);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif

TEST_F(IntegrationTest, InstallId) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), base::Version("1"), false, false,
      base::Version(kUpdaterVersion), "\"iid\":\"my_install_id\""));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&iid=my_install_id"})));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationSansInstallIdTest
    : public ::testing::WithParamInterface<TestUpdaterVersion>,
      public IntegrationTest {};

INSTANTIATE_TEST_SUITE_P(
    IntegrationSansInstallIdTestCases,
    IntegrationSansInstallIdTest,
    ::testing::ValuesIn(GetRealUpdaterLowerVersions("_sans_iid")));

TEST_P(IntegrationSansInstallIdTest, Test) {
  if (!GetParam().version.IsValid()) {
    GTEST_SKIP() << "Skipping test since the version for "
                 << GetParam().updater_setup_path << " is not valid";
  }

  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_TRUE(WaitForUpdaterExit());

  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");

  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), base::Version("1"), false, false,
      GetParam().version, ".*"));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&iid=my_install_id"}),
      /*child_window_text_to_find=*/{}, /*always_launch_cmd=*/false,
      /*verify_app_logo_loaded=*/false, /*expect_success=*/true,
      /*wait_for_the_installer=*/true,
      /*expected_exit_code=*/{},
      /*additional_switches=*/{}));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));

  // Cleanup by overinstalling the current version and uninstalling.
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleWakesOneNetRequest) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Only one sequence visible to the server despite multiple wakes.
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MultipleUpdateAllsMultipleNetRequests) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server, kUpdaterAppId));
  ASSERT_NO_FATAL_FAILURE(UpdateAll());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationGetAppStatesTest : public ::testing::WithParamInterface<bool>,
                                    public IntegrationTest {
 public:
  bool UseLegacyInstallApp() const { return GetParam(); }

  void InstallAppId(const std::string& app_id) {
    if (UseLegacyInstallApp()) {
      FAIL();
    } else {
      ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));
    }
  }
};

INSTANTIATE_TEST_SUITE_P(UseLegacyInstallApp,
                         IntegrationGetAppStatesTest,
                         ::testing::Values(false));

TEST_P(IntegrationGetAppStatesTest, Test) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("0.1");
  if (!UseLegacyInstallApp()) {
    ExpectInstallEvent(test_server, kAppId);
  }
  ASSERT_NO_FATAL_FAILURE(InstallAppId(kAppId));

  if (!UseLegacyInstallApp()) {
    ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));
  }

  base::DictValue expected_app_state;
  expected_app_state.Set("app_id", kAppId);
  expected_app_state.Set("version", v1.GetString());
  expected_app_state.Set("ap", "");
  expected_app_state.Set("brand_code", "");
  expected_app_state.Set("brand_path", "");
  expected_app_state.Set("ecp", "");
  expected_app_state.Set("cohort", "");
  base::DictValue expected_app_states;
  expected_app_states.Set(kAppId, std::move(expected_app_state));

  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, GetAppStates_AppIdsAlwaysLowercase) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());

  base::DictValue expected_app_states;
  for (const std::string appid :
       {"test1", "TEST2", "Test3", "TeSt4", "tEsT5"}) {
    const base::Version v1("0.1");
    ExpectInstallEvent(test_server, appid);
    ASSERT_NO_FATAL_FAILURE(InstallApp(appid));

    ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, v1));

    base::DictValue expected_app_state;
    expected_app_state.Set("app_id", base::ToLowerASCII(appid));
    expected_app_state.Set("version", v1.GetString());
    expected_app_state.Set("ap", "");
    expected_app_state.Set("brand_code", "");
    expected_app_state.Set("brand_path", "");
    expected_app_state.Set("ecp", "");
    expected_app_state.Set("cohort", "");
    expected_app_states.Set(appid, std::move(expected_app_state));
  }

  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallAppInvalidAppId) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(
      "invalid/appid", base::DictValue().Set("expect_failure", true)));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CreateCorrectAndIncorrectScopeProxies) {
  ASSERT_NO_FATAL_FAILURE(Install());

  const std::string kAppId("test");
  const base::Version v1("0.1");
  ASSERT_NO_FATAL_FAILURE(InstallApp(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId, v1));

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectUpdateCheckSequence(
      test_server, kAppId, UpdateService::Priority::kForeground, v1,
      base::Version("1")));

  // Proxy created with the correct scope.
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(kAppId));

  // Proxy created with the opposite scope.
  ASSERT_NO_FATAL_FAILURE(ExpectCheckForUpdateOppositeScopeFails(kAppId));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UnregisterUninstalledApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, UninstallIfMaxServerWakesBeforeRegistrationExceeded) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, UninstallUpdaterWhenAllAppsUninstalled) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(UninstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
}

TEST_F(IntegrationTest, RotateLog) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(FillLog());
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectLogRotated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_P(IntegrationLowerVersionTest, SelfUpdateFromOldReal) {
  ScopedServer test_server(test_commands_);

  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ExpectInstallEvent(test_server, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_P(IntegrationLowerVersionTest, UninstallIfUnusedSelfAndOldReal) {
  ScopedServer test_server(test_commands_);

  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  // Trigger an old instance update check.
  ASSERT_NO_FATAL_FAILURE(ExpectSelfUpdateSequence(test_server));
  ASSERT_NO_FATAL_FAILURE(RunWakeActive(0));

  // Qualify the new instance.
  ExpectInstallEvent(test_server, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(test_server, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Activate the new instance. (It should not check itself for updates.)
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(SetServerStarts(24));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Expect that the updater uninstalled itself as well as the lower version.
}

// Tests that installing and uninstalling an old version of the updater from
// CIPD is possible.
TEST_P(IntegrationLowerVersionTest, InstallLowerVersion) {
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(GetParam().updater_setup_path));
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(Uninstall());

}

#endif  // BUILDFLAG(CHROMIUM_BRANDING) || BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, MAYBE_UpdateServiceStress) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(StressUpdateService());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, IdleServerExits) {
  ASSERT_NO_FATAL_FAILURE(EnterTestMode(
      GURL("http://localhost:1234"), GURL("http://localhost:1234"),
      /*app_logo_url=*/{}, /*event_logging_url=*/{}, base::Seconds(1)));
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, true));
  ASSERT_NO_FATAL_FAILURE(RunServer(kErrorIdle, false));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, SameVersionUpdate) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  ExpectInstallEvent(test_server, app_id);
  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = absl::StrFormat(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"4.0",)"
      R"(  "app":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"("updatecheck":{"sameversionupdate":true},"version":"0.1"}.*)"})},
      response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kAllowed));

  test_server.ExpectOnce({request::GetUpdaterUserAgentMatcher(),
                          request::GetContentMatcher(
                              {R"(.*"updatecheck":{},"version":"0.1"}.*)"})},
                         response);
  ASSERT_NO_FATAL_FAILURE(CallServiceUpdate(
      app_id, "", UpdateService::PolicySameVersionUpdate::kNotAllowed));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, InstallDataIndex) {
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string app_id = "test-appid";
  const std::string install_data_index = "test-install-data-index";

  ExpectInstallEvent(test_server, app_id);
  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id));

  const std::string response = absl::StrFormat(
      ")]}'\n"
      R"({"response":{)"
      R"(  "protocol":"4.0",)"
      R"(  "apps":[)"
      R"(    {)"
      R"(      "appid":"%s",)"
      R"(      "status":"ok",)"
      R"(      "updatecheck":{)"
      R"(        "status":"noupdate")"
      R"(      })"
      R"(    })"
      R"(  ])"
      R"(}})",
      app_id.c_str());

  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {absl::StrFormat(R"(.*"data":\[{"index":"%s","name":"install"}],.*)",
                            install_data_index.c_str())})},
      response);

  ASSERT_NO_FATAL_FAILURE(
      CallServiceUpdate(app_id, install_data_index,
                        UpdateService::PolicySameVersionUpdate::kAllowed));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, MigrateLegacyUpdater) {
  ASSERT_NO_FATAL_FAILURE(SetupFakeLegacyUpdater());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectLegacyUpdaterMigrated());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RecoveryNoUpdater) {
  const std::string appid = "test1";
  const base::Version version("0.1");
  ASSERT_NO_FATAL_FAILURE(RunRecoveryComponent(appid, version));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, version));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RecoveryWithUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  const std::string appid = "test1";
  const base::Version version("0.1");
  ASSERT_NO_FATAL_FAILURE(RunRecoveryComponent(appid, version));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(appid, version));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RecoveryNoAppId) {
  ASSERT_NO_FATAL_FAILURE(RunRecoveryComponent("", {}));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, RegisterApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  RegistrationRequest registration;
  registration.app_id = "e595682b-02d5-46d1-b7ab-90034bd6be0f";
  registration.brand_code = "TSBD";
  registration.brand_path = base::FilePath::FromUTF8Unsafe("/bp");
  registration.ap = "TestAp";
  registration.version = "11.22.33.44";
  registration.existence_checker_path = base::FilePath::FromUTF8Unsafe("/tmp");
  registration.cohort = "cohort_test";
  test_commands_->RegisterApp(registration);

  base::DictValue expected_app_state;
  expected_app_state.Set("app_id", "e595682b-02d5-46d1-b7ab-90034bd6be0f");
  expected_app_state.Set("brand_code", "TSBD");
  expected_app_state.Set("brand_path", "/bp");
  expected_app_state.Set("ap", "TestAp");
  expected_app_state.Set("version", "11.22.33.44");
  expected_app_state.Set("ecp", "/tmp");
#if BUILDFLAG(IS_POSIX)
  // Cohort is only communicated over IPC on POSIX. Refer to crbug.com/40283110.
  expected_app_state.Set("cohort", "cohort_test");
#endif
  base::DictValue expected_app_states;
  expected_app_states.Set("e595682b-02d5-46d1-b7ab-90034bd6be0f",
                          std::move(expected_app_state));
  ASSERT_NO_FATAL_FAILURE(GetAppStates(expected_app_states));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CrashUsageStatsEnabled) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  ScopedServer test_server(test_commands_);
  const std::string response;
  test_server.ExpectOnce(
      {
          request::GetPathMatcher(
              absl::StrFormat(R"(%s\?product=%s&version=%s&guid=.*)",
                              test_server.crash_report_path().c_str(),
                              CRASH_PRODUCT_NAME, kUpdaterVersion)),
          request::GetHeaderMatcher({{"User-Agent", R"(Crashpad/.*)"}}),
          request::GetMultipartContentMatcher({
              {"guid", std::vector<std::string>({})},  // Crash guid.
              {"prod", std::vector<std::string>({CRASH_PRODUCT_NAME})},
              {"ver", std::vector<std::string>({kUpdaterVersion})},
              {"upload_file_minidump",  // Dump file name and its content.
               std::vector<std::string>(
                   {R"(filename=".*dmp")",
                    R"(Content-Type: application/octet-stream)", R"(MDMP)"})},
          }),
      },
      response);
  ExpectUninstallPing(test_server);
  RunCrashMe();
  ASSERT_TRUE(WaitForUpdaterExit());

  // Delete the dmp files generated by this test, so `ExpectNoCrashes` won't
  // complain at TearDown.
  std::optional<base::FilePath> database_path(
      GetCrashDatabasePath(GetUpdaterScopeForTesting()));
  if (database_path && base::PathExists(*database_path)) {
    base::FileEnumerator(*database_path, true, base::FileEnumerator::FILES,
                         FILE_PATH_LITERAL("*.dmp"),
                         base::FileEnumerator::FolderSearchPolicy::ALL)
        .ForEach([](const base::FilePath& name) {
          VLOG(0) << "Deleting file at: " << name;
          EXPECT_TRUE(base::DeleteFile(name));
        });
  }
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CreatesUserReadablePrefs) {
  if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(
      ExpectUserReadable(GetInstallDirectory(GetUpdaterScopeForTesting())
                             ->AppendASCII("prefs.json")));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationTestDeviceManagement : public IntegrationTest {
 protected:
  void SetUp() override {
    if (IsSkipped()) {
      return;
    }
    IntegrationTest::SetUp();
    if (!IsSystemInstall(GetUpdaterScopeForTesting())) {
      GTEST_SKIP();
    }
    test_server_ = std::make_unique<ScopedServer>(test_commands_);
    DMCleanup();
    UninstallEnterpriseCompanionApp();
    ASSERT_NO_FATAL_FAILURE(SetMachineManaged(true));
    ASSERT_TRUE(vapid_test_server_.Start());
    InstallEnterpriseCompanionAppOverrides(
        base::DictValue()
            .Set("crash_upload_url", test_server_->crash_upload_url().spec())
            .Set("dm_encrypted_reporting_url",
                 vapid_test_server_.base_url().spec())
            .Set("dm_realtime_reporting_url",
                 vapid_test_server_.base_url().spec())
            .Set("dm_server_url", test_server_->device_management_url().spec())
            .Set("event_logging_url", vapid_test_server_.base_url().spec()));
  }

  void TearDown() override {
    if (IsSkipped()) {
      return;
    }
    if (IsSystemInstall(GetUpdaterScopeForTesting())) {
      UninstallEnterpriseCompanionApp();
    }
    DMCleanup();
    IntegrationTest::TearDown();
  }

  void SetCloudPolicyOverridesPlatformPolicy() {
// Cloud policy overrides platform policy default, except on Windows.
  }

  std::unique_ptr<ScopedServer> test_server_;

  // A test server that is not configured with any expectations or interesting
  // responses. This is useful for providing addresses to the enterprise
  // companion app for interactions not intended to be covered by these tests.
  net::test_server::EmbeddedTestServer vapid_test_server_;
  static constexpr char kEnrollmentToken[] =
      "00001111-beef-f00d-2222-333344445555";
  static constexpr char kDMToken[] = "integration-dm-token";

  static constexpr char kGlobalPolicyKey[] = "global";
};

// Tests the setup and teardown of the fixture.
TEST_F(IntegrationTestDeviceManagement, Nothing) {}

TEST_F(IntegrationTestDeviceManagement, PolicyFetchBeforeInstall) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  DMPushEnrollmentToken(kEnrollmentToken);

  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken, [&] {
    OmahaSettingsClientProto omaha_settings;
    omaha_settings.set_install_default(
        enterprise_management::INSTALL_DEFAULT_DISABLED);
    omaha_settings.set_download_preference("not-cacheable");
    omaha_settings.set_proxy_mode("system");
    omaha_settings.set_proxy_server("test.proxy.server");
    ApplicationSettings app;
    app.set_app_guid(kApp1.appid);
    app.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
    app.set_target_version_prefix("0.1");
    app.set_rollback_to_target_version(
        enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
    omaha_settings.mutable_application_settings()->Add(std::move(app));
    return omaha_settings;
  }());
  ExpectUpdateCheckRequest(*test_server_);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  scoped_refptr<device_management_storage::DMStorage> dm_storage =
      device_management_storage::GetDefaultDMStorage();
  ASSERT_NE(dm_storage, nullptr);
  std::optional<OmahaSettingsClientProto> omaha_policy =
      GetOmahaPolicySettings(dm_storage);
  ASSERT_TRUE(omaha_policy);
  EXPECT_EQ(omaha_policy->download_preference(), "not-cacheable");
  EXPECT_EQ(omaha_policy->proxy_mode(), "system");
  EXPECT_EQ(omaha_policy->proxy_server(), "test.proxy.server");
  ASSERT_GT(omaha_policy->application_settings_size(), 0);
  const ApplicationSettings& app_policy =
      omaha_policy->application_settings()[0];
  EXPECT_EQ(app_policy.app_guid(), kApp1.appid);
  EXPECT_EQ(app_policy.update(), enterprise_management::AUTOMATIC_UPDATES_ONLY);
  EXPECT_EQ(app_policy.target_version_prefix(), "0.1");
  EXPECT_EQ(app_policy.rollback_to_target_version(),
            enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement,
       PolicyFetchFailedButAppInstalledAnyway) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_);
  ExpectDeviceManagementRequest(*test_server_, "register_policy_agent",
                                "GoogleEnrollmentToken", kEnrollmentToken,
                                net::HTTP_INTERNAL_SERVER_ERROR,
                                [] { return "Test server error"; }());

  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));
  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, PolicyFetchFailedButAppUpdatedAnyway) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ExpectInstallEvent(*test_server_, kApp1.appid);
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ExpectAppInstalled(kApp1.appid, kApp1.v1);

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_);
  ExpectDeviceManagementRequest(*test_server_, "register_policy_agent",
                                "GoogleEnrollmentToken", kEnrollmentToken,
                                net::HTTP_INTERNAL_SERVER_ERROR,
                                [] { return "Test server error"; }());

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/{},
      {AppUpdateExpectation(
          kApp1.GetInstallCommandLineArgs(/*install_v1=*/false), kApp1.appid,
          kApp1.v1, kApp1.v2,
          /*is_install=*/false,
          /*should_update=*/true, false, "", "",
          GetInstallerPath(kApp1.v2_crx))});
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, AppInstall) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_install_default(
      enterprise_management::INSTALL_DEFAULT_DISABLED);
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_install(enterprise_management::INSTALL_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));

  DMPushEnrollmentToken(kEnrollmentToken);
  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      }));

  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp1.appid));

  ExpectDeviceManagementPolicyFetchRequest(
      *test_server_, kDMToken, omaha_settings, /*first_request=*/false);
  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp2.appid));

  // Repeat App2 installation again.
  ExpectDeviceManagementPolicyFetchRequest(
      *test_server_, kDMToken, omaha_settings, /*first_request=*/false);
  ASSERT_NO_FATAL_FAILURE(InstallAppViaService(kApp2.appid));

  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, ForceInstall) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  DMPushEnrollmentToken(kEnrollmentToken);
  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken, [&] {
    // Force-install app1, enable install app2.
    OmahaSettingsClientProto omaha_settings;
    omaha_settings.set_install_default(
        enterprise_management::INSTALL_DEFAULT_DISABLED);
    ApplicationSettings app1;
    app1.set_app_guid(kApp1.appid);
    app1.set_install(enterprise_management::INSTALL_FORCED);
    omaha_settings.mutable_application_settings()->Add(std::move(app1));
    ApplicationSettings app2;
    app2.set_app_guid(kApp2.appid);
    app2.set_install(enterprise_management::INSTALL_ENABLED);
    omaha_settings.mutable_application_settings()->Add(std::move(app2));
    return omaha_settings;
  }());
  ExpectUpdateCheckRequest(*test_server_);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/{},
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
              base::Version({0, 0, 0, 0}), kApp1.v1,
              /*is_install=*/true,
              /*should_update=*/true, false, "", "",
              GetInstallerPath(kApp1.v1_crx)),
      });

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ExpectAppInstalled(kApp1.appid, kApp1.v1);
  ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered(kApp2.appid));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement, QualifyUpdaterWhenUpdateDisabled) {
  // This test depends on the companion app to provide CBCM policies. On macOS
  // the companion app requires a valid ksadmin to install, which the fake
  // updater does not provide.
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(
      GetRealUpdaterLowerVersions().back().updater_setup_path));
  // Install an app to ensure that when the real updater is overinstalled, it
  // does not uninstall all updaters due to appearing unused.
  ExpectInstallEvent(*test_server_, kApp1.appid);
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_update_default(enterprise_management::UPDATES_DISABLED);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);

  // Disable global update via CBCM.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectInstallEvent(*test_server_, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(*test_server_, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the new instance is qualified and activated itself.
  ExpectDeviceManagementPolicyFetchRequest(
      *test_server_, kDMToken, omaha_settings, /*first_request=*/false);
  test_server_->ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher({absl::StrFormat(".*%s.*", kUpdaterAppId)})},
      ")]}'\n");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestDeviceManagement,
       QualifyUpdaterWhenNextCheckDelayIsZero) {
  // This test depends on the companion app to provide CBCM policies. On macOS
  // the companion app requires a valid ksadmin to install, which the fake
  // updater does not provide.
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(SetupRealUpdater(
      GetRealUpdaterLowerVersions().back().updater_setup_path));
  // Install an app to ensure that when the real updater is overinstalled, it
  // does not uninstall all updaters due to appearing unused.
  ExpectInstallEvent(*test_server_, kApp1.appid);
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionNotActive(kUpdaterVersion));

  OmahaSettingsClientProto omaha_settings;
  omaha_settings.set_auto_update_check_period_minutes(0);
  omaha_settings.set_cloud_policy_overrides_platform_policy(true);

  // Set update check period to zero via CBCM.
  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectInstallEvent(*test_server_, kQualificationAppId);
  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken,
                                           omaha_settings);
  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateSequence(*test_server_, kQualificationAppId, "",
                           UpdateService::Priority::kBackground,
                           base::Version("0.1"), base::Version("0.2")));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Verify the new instance is qualified and activated itself.
  ExpectDeviceManagementPolicyFetchRequest(
      *test_server_, kDMToken, omaha_settings, /*first_request=*/false);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// During the updater's installation and periodic tasks, the enterprise
// companion app should not be installed if the device is not cloud managed.
TEST_F(IntegrationTestDeviceManagement, FetchPolicy_SkipCompanionAppInstall) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());

  ExpectUpdateCheckRequest(*test_server_);
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)
TEST_F(IntegrationTestDeviceManagement,
       UninstallCompanionAppWhenUninstallUpdater) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      *test_server_, kApp1.appid, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), kApp1.v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kApp1.appid, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kApp1.appid, "&usagestats=1"}),
      /*child_window_text_to_find=*/{}, /*always_launch_cmd=*/false,
      /*verify_app_logo_loaded=*/false, /*expect_success=*/true,
      /*wait_for_the_installer=*/true,
      /*expected_exit_code=*/{},
      /*additional_switches=*/{}));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallEnterpriseCompanionApp());

  // Uninstall ping for the app.
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  // Expect an update check and then the uninstall ping for the updater itself.
  ExpectUpdateCheckRequest(*test_server_);
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectEnterpriseCompanionAppNotInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif

class IntegrationTestCloudPolicyOverridesPlatformPolicy
    : public ::testing::WithParamInterface<bool>,
      public IntegrationTestDeviceManagement {};

TEST_P(IntegrationTestCloudPolicyOverridesPlatformPolicy, UseCloudPolicy) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install(/*switches=*/{}));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ExpectInstallEvent(*test_server_, kApp1.appid);
  ExpectInstallEvent(*test_server_, kApp2.appid);
  ExpectInstallEvent(*test_server_, kApp3.appid);
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp2, /*install_v1=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp3, /*install_v1=*/true));

  base::DictValue policies;
  policies.Set(kGlobalPolicyKey, base::DictValue()
                                     .Set("UpdateDefault", kPolicyDisabled)
                                     .Set("DownloadPreference", "cacheable"));
  policies.Set(kApp1.appid, base::DictValue()
                                .Set("Update", kPolicyDisabled)
                                .Set("TargetChannel", "beta"));
  policies.Set(kApp2.appid, base::DictValue().Set("Update", kPolicyEnabled));
  policies.Set(kApp3.appid, base::DictValue()
                                .Set("Update", kPolicyEnabled)
                                .Set("TargetChannel", "canary"));
  ASSERT_NO_FATAL_FAILURE(SetPlatformPolicies(policies));

  // Overrides app1 to auto-update, app2 to manual-update with cloud policy.
  DMPushEnrollmentToken(kEnrollmentToken);
  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app1;
  app1.set_app_guid(kApp1.appid);
  app1.set_update(enterprise_management::AUTOMATIC_UPDATES_ONLY);
  app1.set_target_channel("beta_canary");
  omaha_settings.mutable_application_settings()->Add(std::move(app1));
  ApplicationSettings app2;
  app2.set_app_guid(kApp2.appid);
  app2.set_update(enterprise_management::MANUAL_UPDATES_ONLY);
  omaha_settings.mutable_application_settings()->Add(std::move(app2));
  if (GetParam()) {
    omaha_settings.set_cloud_policy_overrides_platform_policy(true);
  } else {
    ASSERT_NO_FATAL_FAILURE(SetCloudPolicyOverridesPlatformPolicy());
  }

  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken,
                                           omaha_settings);
  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/base::DictValue().Set("dlpref", "cacheable"),
      {
          AppUpdateExpectation(
              kApp1.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp1.appid, kApp1.v1, kApp1.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "beta_canary",
              GetInstallerPath(kApp1.v2_crx)),
          AppUpdateExpectation(
              kApp2.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp2.appid, kApp2.v1, kApp2.v1,
              /*is_install=*/false,
              /*should_update=*/false, false, "", "",
              GetInstallerPath(kApp2.v2_crx)),
          AppUpdateExpectation(
              kApp3.GetInstallCommandLineArgs(/*install_v1=*/false),
              kApp3.appid, kApp3.v1, kApp3.v2,
              /*is_install=*/false,
              /*should_update=*/true, false, "", "canary",
              GetInstallerPath(kApp3.v2_crx)),
      });
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp2.appid, kApp2.v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp3.appid, kApp3.v2));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp2.appid));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp3.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

INSTANTIATE_TEST_SUITE_P(
    IntegrationTestCloudPolicyOverridesPlatformPolicyTestCases,
    IntegrationTestCloudPolicyOverridesPlatformPolicy,
    ::testing::Bool());

TEST_F(IntegrationTestDeviceManagement, RollbackToTargetVersion) {
  constexpr char kTargetVersionPrefix[] = "1.0.";
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kApp1.appid);
  ASSERT_NO_FATAL_FAILURE(InstallTestApp(kApp1, /*install_v1=*/false));

  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v2));

  ASSERT_NO_FATAL_FAILURE(
      ExpectEnterpriseCompanionAppOTAInstallSequence(*test_server_));

  DMPushEnrollmentToken(kEnrollmentToken);
  ExpectDeviceManagementRegistrationRequest(*test_server_, kEnrollmentToken,
                                            kDMToken);
  OmahaSettingsClientProto omaha_settings;
  ApplicationSettings app;
  app.set_app_guid(kApp1.appid);
  app.set_target_version_prefix(kTargetVersionPrefix);
  app.set_rollback_to_target_version(
      enterprise_management::ROLLBACK_TO_TARGET_VERSION_ENABLED);
  omaha_settings.mutable_application_settings()->Add(std::move(app));
  ExpectDeviceManagementPolicyFetchRequest(*test_server_, kDMToken,
                                           omaha_settings);

  ExpectAppsUpdateSequence(
      UpdaterScope::kSystem, *test_server_,
      /*request_attributes=*/{},
      {AppUpdateExpectation(
          kApp1.GetInstallCommandLineArgs(/*install_v1=*/true), kApp1.appid,
          kApp1.v2, kApp1.v1,
          /*is_install=*/false,
          /*should_update=*/true, /*allow_rollback=*/true, kTargetVersionPrefix,
          "", GetInstallerPath(kApp1.v1_crx))});
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppInstalled(kApp1.appid, kApp1.v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(UninstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Tests that interact with state in both system and user updater configuration
// are run as part of the system-scope tests.
class IntegrationTestUserInSystem : public IntegrationTest {
 protected:
  void SetUp() override {
    if (SkipTest()) {
      GTEST_SKIP() << "The test is skipped in this configuration";
    }

    IntegrationTest::SetUp();

    for (auto commands : {test_commands_, user_test_commands_}) {
      commands->EnterTestMode(
          test_server_->update_url(), test_server_->crash_upload_url(),
          /*app_logo_url=*/{},
          /*event_logging_url=*/{}, base::Minutes(5), base::Seconds(2),
          base::Seconds(10),
          /*event_logging_permission_provider=*/std::nullopt);
    }
  }

  void TearDown() override {
    if (SkipTest()) {
      return;
    }
    IntegrationTest::TearDown();
  }

  void InstallUserUpdater() { user_test_commands_->Install(base::ListValue()); }

  void UninstallUserUpdater() {
    ASSERT_TRUE(WaitForUpdaterExit());
    ExpectNoCrashes();
    PrintUserLog();
    CopyUserLog();
    user_test_commands_->Uninstall();
    ASSERT_TRUE(WaitForUpdaterExit());
  }

  void ExpectUserUpdaterInstalled() { user_test_commands_->ExpectInstalled(); }

  void InstallUserApp(const std::string& app_id, const base::Version& version) {
    user_test_commands_->InstallApp(app_id, version);
  }

  void ExpectUserAppVersion(const std::string& app_id,
                            const base::Version& version) {
    user_test_commands_->ExpectAppVersion(app_id, version);
  }

  void SetUserAppExistenceCheckerPath(const std::string& app_id,
                                      const base::FilePath& path) {
    user_test_commands_->SetExistenceCheckerPath(app_id, path);
  }

  void SetUserAppTag(const std::string& app_id, const std::string& tag) {
    user_test_commands_->SetAppTag(app_id, tag);
  }

  void ExpectUserAppTag(const std::string& app_id, const std::string& tag) {
    user_test_commands_->ExpectAppTag(app_id, tag);
  }

  void PrintUserLog() { user_test_commands_->PrintLog(); }

  void CopyUserLog() { user_test_commands_->CopyLog("user"); }

  void ExpectUserUninstallPing(ScopedServer& test_server,
                               std::optional<GURL> target_url = {}) {
    user_test_commands_->ExpectPing(
        test_server, update_client::protocol_request::kEventUninstall,
        target_url);
  }

#if BUILDFLAG(IS_MAC)
  void ExpectUserCRURegistrationChecksForUpdate(
      const std::string& app_id,
      const base::FilePath& xc_path,
      const std::string& expected_version) {
    user_test_commands_->ExpectCRURegistrationChecksForUpdate(app_id, xc_path,
                                                              expected_version);
  }
#endif  // BUILDFLAG(IS_MAC)

  void ExpectUserInstallSequence(ScopedServer& test_server,
                                 const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version) {
    user_test_commands_->ExpectInstallSequence(
        test_server, app_id, install_data_index, priority, from_version,
        to_version,
        /*do_fault_injection=*/false,
        /*skip_download=*/false,
        /*updater_version=*/base::Version(kUpdaterVersion),
        /*event_regex=*/".*");
  }

  void InstallUserUpdaterAndApp(
      const std::string& app_id,
      const bool is_silent_install,
      const std::string& tag,
      const std::string& child_window_text_to_find = {},
      const bool always_launch_cmd = false,
      const bool verify_app_logo_loaded = false) {
    user_test_commands_->InstallUpdaterAndApp(
        app_id, is_silent_install, tag, child_window_text_to_find,
        always_launch_cmd, verify_app_logo_loaded,
        /*expect_success=*/true, /*wait_for_the_installer=*/true,
        /*expected_exit_code=*/{},
        /*additional_switches=*/{}, /*updater_path=*/GetSetupExecutablePath());
  }

  scoped_refptr<IntegrationTestCommands> user_test_commands_ =
      CreateIntegrationTestCommandsUser(UpdaterScope::kUser);
  std::unique_ptr<ScopedServer> test_server_ =
      std::make_unique<ScopedServer>(test_commands_);

 private:
  // Even though the updater itself supports installing per-user applications at
  // high integrity, most of the tests in the `IntegrationTestUserInSystem` test
  // suite cannot run on Windows with UAC on, because the integration test
  // driver does not fully support installing per-user applications at high
  // integrity. For instance, it functions as a COM client running at high
  // integrity to create the user updater COM server, which is not supported on
  // Windows with UAC on.
  bool SkipTest() const {
    return !IsSystemInstall(GetUpdaterScopeForTesting()) ||
           (WrongUser(UpdaterScope::kUser) &&
            (GetTestName() !=
             "IntegrationTestUserInSystem.ElevatedInstallOfUserUpdaterAndApp"));
  }
};

// Tests the updater's functionality of installing per-user applications at high
// integrity. This test uses integration test driver APIs that support
// installing per-user applications at high integrity. For instance, it runs
// `UpdaterSetup --install --app-id=test` and `UpdaterSetup --uninstall`
// elevated via the command line, so that it directly uses the updater's
// functionality of de-elevating.
TEST_F(IntegrationTestUserInSystem, ElevatedInstallOfUserUpdaterAndApp) {
  const std::string kAppId("test");
  const base::Version v1("1");
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectUserInstallSequence(
      *test_server_, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));

  ASSERT_NO_FATAL_FAILURE(InstallUserUpdaterAndApp(
      kAppId, /*is_silent_install=*/true, "usagestats=1"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectUserAppVersion(kAppId, v1));

  ASSERT_NO_FATAL_FAILURE(ExpectUserUninstallPing(*test_server_));
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
}

TEST_F(IntegrationTestUserInSystem, TagNonInterference) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  ExpectInstallEvent(*test_server_, "test_app");
  base::Version v("1.0.0.0");
  ASSERT_NO_FATAL_FAILURE(InstallApp("test_app", v));
  ExpectAppVersion("test_app", v);
  ExpectAppTag("test_app", "");
  ExpectInstallEvent(*test_server_, "test_app");
  ASSERT_NO_FATAL_FAILURE(InstallUserApp("test_app", v));
  ExpectUserAppVersion("test_app", v);
  ExpectUserAppTag("test_app", "");

  ASSERT_NO_FATAL_FAILURE(SetAppTag("test_app", "system"));
  ExpectAppTag("test_app", "system");
  ExpectUserAppTag("test_app", "");
  ASSERT_NO_FATAL_FAILURE(SetUserAppTag("test_app", "user"));
  ExpectUserAppTag("test_app", "user");
  ExpectAppTag("test_app", "system");

  ExpectUninstallPing(*test_server_);
  Uninstall();
  ExpectUserUninstallPing(*test_server_);
  UninstallUserUpdater();
}

// macOS specific tests.
#if BUILDFLAG(IS_MAC)

// The CRURegistration library exists only on macOS. It runs ksadmin. It should
// not find ksadmin before the updater is installed or after it is uninstalled,
// but should find the scope-suitable ksadmin while the updater is installed.
TEST_F(IntegrationTest, CRURegistrationFindKSAdmin) {
  EXPECT_NO_FATAL_FAILURE(ExpectCRURegistrationCannotFindKSAdmin())
      << "ksadmin found before first installation.";
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFindsKSAdmin(GetUpdaterScopeForTesting()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
  EXPECT_NO_FATAL_FAILURE(ExpectCRURegistrationCannotFindKSAdmin())
      << "ksadmin found after uninstall.";
}

TEST_F(IntegrationTest, CRURegistrationCannotGetTagWithoutUpdater) {
  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationCannotFetchTag(kApp1.appid, xc_path.path()));
}

TEST_F(IntegrationTest, CRURegistrationCannotGetTagWithoutApp) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationCannotFetchTag(kApp1.appid, xc_path.path()));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)
TEST_F(IntegrationTest, CRURegistrationFindsBlankTag) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());

  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());
  ASSERT_NO_FATAL_FAILURE(InstallApp(kApp1.appid));
  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(kApp1.appid, xc_path.path()));

  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFetchesTag(kApp1.appid, xc_path.path(), ""));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationFindsTag) {
  ScopedServer test_server(test_commands_);
  const std::string kAppId("test");
  const base::Version v1("1");
  base::ScopedTempFile xc_path;
  ASSERT_TRUE(xc_path.Create());

  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId, "&ap=tagvalue&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(kAppId, xc_path.path()));

  EXPECT_NO_FATAL_FAILURE(
      ExpectCRURegistrationFetchesTag(kAppId, xc_path.path(), "tagvalue"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // !defined(ADDRESS_SANITIZER)

// App ownership feature only exists on macOS.
TEST_F(IntegrationTest, UnregisterUnownedApp) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));

  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(
      "test1", IsSystemInstall(GetUpdaterScopeForTesting())
                   ? temp_dir.GetPath()
                   : GetDifferentUserPath()));

  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());

  // Since the updater may have chowned the temp dir, we may need to elevate to
  // delete it.
  ASSERT_NO_FATAL_FAILURE(DeleteFile(temp_dir.GetPath()));

  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  } else {
    ASSERT_NO_FATAL_FAILURE(ExpectNotRegistered("test1"));
  }

  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test2"));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// The updater shims are only repaired by the server on macOS.
TEST_F(IntegrationTest, RepairUpdater) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(DeleteLegacyUpdater());
  std::optional<base::FilePath> ksadmin_path =
      GetKSAdminPath(GetUpdaterScopeForTesting());
  ASSERT_TRUE(ksadmin_path.has_value());
  ASSERT_FALSE(base::PathExists(*ksadmin_path));
  ASSERT_NO_FATAL_FAILURE(RunWake(0));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_TRUE(base::PathExists(*ksadmin_path));
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// Only macOS software needs to try to suppress user-visible Gatekeeper popups.
TEST_F(IntegrationTest, SmokeTestPrepareToRunBundle) {
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectVersionActive(kUpdaterVersion));
  ASSERT_TRUE(WaitForUpdaterExit());

  std::optional<base::FilePath> updater_path =
      GetUpdaterAppBundlePath(GetUpdaterScopeForTesting());
  ASSERT_TRUE(updater_path);
  ASSERT_NO_FATAL_FAILURE(ExpectPrepareToRunBundleSuccess(*updater_path));

  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// The privileged helper only exists on macOS. This does not test installation
// of the helper itself, but is meant to cover its core functionality.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(IntegrationTest, PrivilegedHelperInstall) {
  if (GetUpdaterScopeForTesting() != UpdaterScope::kSystem) {
    return;  // Test is only applicable to system scope.
  }
  ASSERT_NO_FATAL_FAILURE(PrivilegedHelperInstall());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectRegistered("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion("test1", base::Version("1.2.3.4")));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(IntegrationTest, FallbackToOutOfProcessFetcher) {
  const std::string kAppId1("test1");
  const base::Version v1("1");
  // Injects an HTTP error before each network fetch to activate the fallback
  // fetcher. The installation should still succeed.
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_server, kAppId1, "", UpdateService::Priority::kForeground,
      base::Version({0, 0, 0, 0}), v1, /*do_fault_injection=*/true));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId1, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId1, "&ap=foo&usagestats=1"})));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId1, v1));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId1, "foo"));

  const std::string kAppId2("test2");
  const base::Version v2("2.0");
  // Consecutive HTTP errors should fail the installation, given the fact that
  // updater has only one fallback for each network task.
  test_server.ExpectOnce({}, "", net::HTTP_INTERNAL_SERVER_ERROR);
  test_server.ExpectOnce({}, "", net::HTTP_GONE);
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      kAppId2, /*is_silent_install=*/true,
      base::StrCat({"appguid=", kAppId2, "&ap=foo2&usagestats=1"}),
      /*child_window_text_to_find=*/{}, /*always_launch_cmd=*/false,
      /*verify_app_logo_loaded=*/false,
      /*expect_success=*/false,
      /*wait_for_the_installer=*/true,
      /*expected_exit_code=*/5));
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectAppVersion(kAppId2, base::Version()));
  ASSERT_NO_FATAL_FAILURE(ExpectAppTag(kAppId2, ""));
  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, KSAdminNoAppNoTag) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
    // Actually, since this test expects ksadmin to fail, it passes under these
    // conditions, but for the wrong reason.
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ExpectKSAdminFetchTag(false, "no.such.app", {}, {}, {});
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, KSAdminUntaggedApp) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("org.chromium.testapp"));
  ExpectKSAdminFetchTag(false, "org.chromium.testapp", {}, {}, "");
  ASSERT_NO_FATAL_FAILURE(UninstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, KSAdminTaggedApp) {
#if defined(ADDRESS_SANITIZER)
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP() << "User->System launcher can't load macOS ASAN dylib.";
  }
#else
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(InstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(SetAppTag("org.chromium.testapp", "some-tag"));
  ExpectKSAdminFetchTag(false, "org.chromium.testapp", {}, {}, "some-tag");
  ASSERT_NO_FATAL_FAILURE(UninstallApp("org.chromium.testapp"));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
#endif  // defined(ADDRESS_SANITIZER)
}

TEST_F(IntegrationTest, CRURegistrationInstallsUpdater) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppUserUpdaterInstallSuccess());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ExpectUninstallPing(test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationIdempotentInstallSuccess) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppUserUpdaterInstallSuccess());
  ExpectInstalled();

  ExpectUninstallPing(test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationRegister) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ExpectInstallEvent(test_server,
                     "org.chromium.CRURegistration.testing.RegisterMe");
  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppRegisterSuccess());
  ExpectAppVersion("org.chromium.CRURegistration.testing.RegisterMe",
                   base::Version({1, 0, 0, 0}));

  ExpectUninstallPing(test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationInstallAndRegister) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ScopedServer test_server(test_commands_);
  ExpectInstallEvent(test_server, kUpdaterAppId);
  ExpectInstallEvent(test_server,
                     "org.chromium.CRURegistration.testing.RegisterMe");
  ASSERT_NO_FATAL_FAILURE(ExpectRegistrationTestAppInstallAndRegisterSuccess());
  ExpectInstalled();
  ExpectAppVersion("org.chromium.CRURegistration.testing.RegisterMe",
                   base::Version({2, 0, 0, 0}));

  ExpectUninstallPing(test_server);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// This is a copy of ReportsActive, but it uses CRURegistration to mark the
// app active. If both this test and ReportsActive fail, suspect an issue with
// actives reporting; if only this test fails, suspect CRURegistration.
TEST_F(IntegrationTest, CRURegistrationReportsActive) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  // A longer than usual timeout is needed for this test because the macOS
  // UpdateServiceInternal server takes at least 10 seconds to shut down after
  // Install, and InstallApp cannot make progress until it shut downs and
  // releases the global prefs lock.
  ASSERT_GE(TestTimeouts::action_timeout(), base::Seconds(18));
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());

  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  // Register apps test1 and test2. Expect pings for each.
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test2"));

  // Set test1 to be active via CRURegistration and do a background updatecheck.
  ASSERT_NO_FATAL_FAILURE(ExpectCRURegistrationMarksActive(
      "test1", test_commands_->GetNonExistentPath()));
  ASSERT_NO_FATAL_FAILURE(ExpectActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));
  ScopedServer test_server(test_commands_);
  test_server.ExpectOnce(
      {request::GetUpdaterUserAgentMatcher(),
       request::GetContentMatcher(
           {R"(.*"appid":"test1","enabled":true,"installdate":-1,)",
            R"("ping":{"ad":-1,.*)"})},
      R"()]}')"
      "\n"
      R"({"response":{"protocol":"4.0","daystart":{"elapsed_)"
      R"(days":5098}},"apps":[{"appid":"test1","status":"ok",)"
      R"("updatecheck":{"status":"noupdate"}},{"appid":"test2",)"
      R"("status":"ok","updatecheck":{"status":"noupdate"}}]})");
  ASSERT_NO_FATAL_FAILURE(RunWake(0));

  // The updater has cleared the active bits.
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test1"));
  ASSERT_NO_FATAL_FAILURE(ExpectNotActive("test2"));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTest, CRURegistrationChecksForUpdate) {
  if (IsSystemInstall(GetUpdaterScopeForTesting())) {
    GTEST_SKIP();
  }
  ASSERT_NO_FATAL_FAILURE(Install());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());

  ScopedServer test_server(test_commands_);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallEvent(test_server, "test1", 1, "0.1"));
  ASSERT_NO_FATAL_FAILURE(InstallApp("test1"));
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());
  ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath("test1", xc_file.path()));
  ASSERT_NO_FATAL_FAILURE(ExpectNoUpdateSequence(test_server, "test1",
                                                 base::Version(kUpdaterVersion),
                                                 base::Version("0.1")));
  ASSERT_NO_FATAL_FAILURE(
      ExpectCRURegistrationChecksForUpdate("test1", xc_file.path(), ""));

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#if !defined(ADDRESS_SANITIZER)

TEST_F(IntegrationTestUserInSystem, CRURegistrationRegistersApp) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectInstallEvent(*test_server_, "test");
  ExpectCRURegistrationRegisters("test", xc_file.path(), "0.0.0.1");
  ExpectUserAppVersion("test", base::Version({0, 0, 0, 1}));
  ExpectNotRegistered("test");

  ExpectUserUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationUpdatesVersion) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectInstallEvent(*test_server_, "test");
  InstallUserApp("test", base::Version({0, 0, 0, 1}));
  ExpectCRURegistrationRegisters("test", xc_file.path(), "0.0.0.2");
  ExpectUserAppVersion("test", base::Version({0, 0, 0, 2}));
  ExpectNotRegistered("test");

  ExpectUserUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationNeedsUpdater) {
  base::ScopedTempFile xc_file;
  ASSERT_TRUE(xc_file.Create());

  ExpectCRURegistrationCannotRegister("test", xc_file.path(), "0.0.0.1");
}

TEST_F(IntegrationTestUserInSystem, CRURegistrationChecksForUpdateUserApp) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  base::ScopedTempFile user_xc_file;
  ASSERT_TRUE(user_xc_file.Create());
  const std::string app_id = "test-user-app";

  ExpectInstallEvent(*test_server_, app_id);
  ASSERT_NO_FATAL_FAILURE(InstallUserApp(app_id, base::Version({0, 0, 0, 1})));
  ASSERT_NO_FATAL_FAILURE(user_test_commands_->SetExistenceCheckerPath(
      app_id, user_xc_file.path()));

  ExpectNoUpdateSequence(*test_server_, app_id, base::Version(kUpdaterVersion),
                         base::Version({0, 0, 0, 1}));
  ASSERT_NO_FATAL_FAILURE(ExpectUserCRURegistrationChecksForUpdate(
      app_id, user_xc_file.path(), ""));

  ExpectUserUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(IntegrationTestUserInSystem,
       CRURegistrationChecksForUpdateScenarioSystem) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  base::ScopedTempFile system_xc_file;
  ASSERT_TRUE(system_xc_file.Create());
  const std::string app_id = "test-system-app";

  ExpectInstallEvent(*test_server_, app_id);
  ASSERT_NO_FATAL_FAILURE(InstallApp(app_id, base::Version({0, 0, 0, 1})));
  ASSERT_NO_FATAL_FAILURE(
      SetExistenceCheckerPath(app_id, system_xc_file.path()));

  ExpectNoUpdateSequence(*test_server_, app_id, base::Version(kUpdaterVersion),
                         base::Version({0, 0, 0, 1}));
  ASSERT_NO_FATAL_FAILURE(ExpectUserCRURegistrationChecksForUpdate(
      app_id, system_xc_file.path(), ""));

  ExpectUserUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

class IntegrationTestKSAdminUserInSystem : public IntegrationTestUserInSystem {
 protected:
  void ExpectUserKSAdminFetchTag(bool elevate,
                                 const std::string& product_id,
                                 const base::FilePath& xc_path,
                                 std::optional<UpdaterScope> store_flag,
                                 std::optional<std::string> want_tag) {
    user_test_commands_->ExpectKSAdminFetchTag(elevate, product_id, xc_path,
                                               store_flag, want_tag);
  }

  void ExpectBothKSAdminFetchTag(bool elevate,
                                 const std::string& product_id,
                                 const base::FilePath xc_path,
                                 std::optional<UpdaterScope> store_flag,
                                 std::optional<std::string> want_tag) {
    ExpectUserKSAdminFetchTag(elevate, product_id, xc_path, store_flag,
                              want_tag);
    ExpectKSAdminFetchTag(elevate, product_id, xc_path, store_flag, want_tag);
  }
};

TEST_F(IntegrationTestKSAdminUserInSystem, KSAdminNoAppNoTagNoMatterWhat) {
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(Install());
  ExpectInstallEvent(*test_server_, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
  ASSERT_TRUE(WaitForUpdaterExit());
  ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
  ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

  ExpectBothKSAdminFetchTag(false, "no.such.app", {}, {}, {});
  ExpectBothKSAdminFetchTag(true, "no.such.app", {}, {}, {});

  ExpectUserUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
  ExpectUninstallPing(*test_server_);
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

// A set of KSAdmin tests that require apps to be installed in a specific way:
//
// * product ID `system-app`, tag `system-tag`, installed at system scope,
//     version 1.0.0.0
// * product ID `user-app`, tag `user-tag`, installed at user scope,
//     version 1.1.1.1
// * product ID `repeat-app`, tag `repeat-system-tag`, installed at system scope
//     version 1.2.2.2
// * product ID `repeat-app`, tag `repeat-user-tag`, installed at user scope
//     version 1.3.3.3
//
// Each installation has a unique existence checker path referring to a temp
// file created during test setup and deleted during teardown. Test setup and
// teardown also installs and uninstalls updaters at both user and system scope.
//
// Tests may also rely on `nonexistent-app` to test product IDs not registered
// with any updater. The class also provides an extra temp file that is not
// the existence checker path of anything, for similar reasons.
class IntegrationTestKSAdminFourApps
    : public IntegrationTestKSAdminUserInSystem {
 protected:
  void SetUp() override {
    IntegrationTestKSAdminUserInSystem::SetUp();
    if (IsSkipped() || HasFailure()) {
      // If the test should not run, stop without installing the updater.
      return;
    }

    ExpectInstallEvent(*test_server_, kUpdaterAppId);
    ASSERT_NO_FATAL_FAILURE(Install());
    ExpectInstallEvent(*test_server_, kUpdaterAppId);
    ASSERT_NO_FATAL_FAILURE(InstallUserUpdater());
    ASSERT_TRUE(WaitForUpdaterExit());
    ASSERT_NO_FATAL_FAILURE(ExpectInstalled());
    ASSERT_NO_FATAL_FAILURE(ExpectUserUpdaterInstalled());

    ExpectInstallEvent(*test_server_, kSystemAppID);
    ASSERT_NO_FATAL_FAILURE(
        InstallApp(kSystemAppID, base::Version(kSystemAppVersionStr)));
    ASSERT_NO_FATAL_FAILURE(SetAppTag(kSystemAppID, kSystemAppTag));
    ASSERT_TRUE(system_app_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(
        SetExistenceCheckerPath(kSystemAppID, system_app_xcfile_.path()));

    ExpectInstallEvent(*test_server_, kRepeatAppID);
    ASSERT_NO_FATAL_FAILURE(
        InstallApp(kRepeatAppID, base::Version(kRepeatAppSystemVersionStr)));
    ASSERT_NO_FATAL_FAILURE(SetAppTag(kRepeatAppID, kRepeatAppSystemTag));
    ASSERT_TRUE(repeat_app_system_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(SetExistenceCheckerPath(
        kRepeatAppID, repeat_app_system_xcfile_.path()));

    ExpectInstallEvent(*test_server_, kUserAppID);
    ASSERT_NO_FATAL_FAILURE(
        InstallUserApp(kUserAppID, base::Version(kUserAppVersionStr)));
    ASSERT_NO_FATAL_FAILURE(SetUserAppTag(kUserAppID, kUserAppTag));
    ASSERT_TRUE(user_app_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(
        SetUserAppExistenceCheckerPath(kUserAppID, user_app_xcfile_.path()));

    ExpectInstallEvent(*test_server_, kRepeatAppID);
    ASSERT_NO_FATAL_FAILURE(
        InstallUserApp(kRepeatAppID, base::Version(kRepeatAppUserVersionStr)));
    ASSERT_NO_FATAL_FAILURE(SetUserAppTag(kRepeatAppID, kRepeatAppUserTag));
    ASSERT_TRUE(repeat_app_user_xcfile_.Create());
    ASSERT_NO_FATAL_FAILURE(SetUserAppExistenceCheckerPath(
        kRepeatAppID, repeat_app_user_xcfile_.path()));

    ASSERT_TRUE(no_app_xcfile_.Create());
  }

  void TearDown() override {
    if (IsSkipped()) {
      // Did not set up; no setup actions to reverse.
      return;
    }
    ExpectUserUninstallPing(*test_server_);
    ASSERT_NO_FATAL_FAILURE(UninstallUserUpdater());
    ExpectUninstallPing(*test_server_);
    ASSERT_NO_FATAL_FAILURE(Uninstall());
    IntegrationTestKSAdminUserInSystem::TearDown();
  }

  static constexpr char kSystemAppID[] = "system-app";
  static constexpr char kSystemAppTag[] = "system-tag";
  static constexpr char kSystemAppVersionStr[] = "1.0.0.0";
  base::ScopedTempFile system_app_xcfile_;

  static constexpr char kRepeatAppID[] = "repeat-app";
  static constexpr char kRepeatAppSystemTag[] = "repeat-system-tag";
  static constexpr char kRepeatAppSystemVersionStr[] = "1.2.2.2";
  base::ScopedTempFile repeat_app_system_xcfile_;
  static constexpr char kRepeatAppUserTag[] = "repeat-user-tag";
  static constexpr char kRepeatAppUserVersionStr[] = "1.3.3.3";
  base::ScopedTempFile repeat_app_user_xcfile_;

  static constexpr char kUserAppID[] = "user-app";
  static constexpr char kUserAppTag[] = "user-tag";
  static constexpr char kUserAppVersionStr[] = "1.1.1.1";
  base::ScopedTempFile user_app_xcfile_;

  static constexpr char kNonexistentAppID[] = "nonexistent-app";
  base::ScopedTempFile no_app_xcfile_;
};

TEST_F(IntegrationTestKSAdminFourApps, ServiceTagSmokeTest) {
  ExpectAppTag(kSystemAppID, kSystemAppTag);
  ExpectAppTag(kRepeatAppID, kRepeatAppSystemTag);
  ExpectUserAppTag(kUserAppID, kUserAppTag);
  ExpectUserAppTag(kRepeatAppID, kRepeatAppUserTag);
}

TEST_F(IntegrationTestKSAdminFourApps, UserLookupNoHints) {
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, {}, kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, {}, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, {}, kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, {}, {});
}

TEST_F(IntegrationTestKSAdminFourApps, ElevatedLookupNoHints) {
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, {}, kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, {}, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, {}, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, {}, {});
}

TEST_F(IntegrationTestKSAdminFourApps, UserStoreFlag) {
  // When running elevated, ksadmin refuses to use a user store.
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // In the presence of a user store flag, only search the user store.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, UpdaterScope::kUser,
                            kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, UpdaterScope::kUser,
                            kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // Existence checker path hinting does not alter any part of this result.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kUser, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kUser, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kUser, {});
}

TEST_F(IntegrationTestKSAdminFourApps,
       CRURegistrationChecksForUpdateAmbiguousShouldBeUser) {
  ExpectNoUpdateSequence(*test_server_, kRepeatAppID,
                         base::Version(kUpdaterVersion),
                         base::Version(kRepeatAppUserVersionStr));
  ASSERT_NO_FATAL_FAILURE(ExpectUserCRURegistrationChecksForUpdate(
      kRepeatAppID, repeat_app_user_xcfile_.path(), ""));
}

TEST_F(IntegrationTestKSAdminFourApps,
       CRURegistrationChecksForUpdateAmbiguousShouldBeSystem) {
  ExpectNoUpdateSequence(*test_server_, kRepeatAppID,
                         base::Version(kUpdaterVersion),
                         base::Version(kRepeatAppSystemVersionStr));
  // CRURegistration always runs as user. This test verifies that ksadmin
  // correctly deduces the system ticket and system updater anyway.
  ASSERT_NO_FATAL_FAILURE(ExpectUserCRURegistrationChecksForUpdate(
      kRepeatAppID, repeat_app_system_xcfile_.path(), ""));
}

TEST_F(IntegrationTestKSAdminFourApps, SystemStoreFlag) {
  // In the presence of a system store flag, only search the system store.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, {}, UpdaterScope::kSystem,
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, {}, UpdaterScope::kSystem,
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kUserAppID, {}, UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});
  ExpectBothKSAdminFetchTag(true, kSystemAppID, {}, UpdaterScope::kSystem,
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, {}, UpdaterScope::kSystem,
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, {}, UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, {}, UpdaterScope::kUser,
                            {});

  // Existence checker path hinting does not alter elevated results.
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
}

// TODO: crbug/355246092 - Fix ksadmin's handling of this scenario and enable
//     this test. Currently, ksadmin will see the `--system-store` switch and
//     retrieve the registration from the system store, but not check further
//     to verify the existence checker path match.
TEST_F(IntegrationTestKSAdminFourApps,
       DISABLED_SystemStoreFlagXCPathMismatchAsUser) {
  // Because a non-elevated user can't "fix" a mismatched path for a system
  // app registration, a mismatching existence checker path causes lookup
  // to fail; because the store was explicitly specified, ksadmin will not
  // consider the user store.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(),
                            UpdaterScope::kSystem, kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            UpdaterScope::kSystem, {});
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(),
                            UpdaterScope::kSystem, {});
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMatch) {
  ExpectBothKSAdminFetchTag(true, kSystemAppID, system_app_xcfile_.path(), {},
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(false, kSystemAppID, system_app_xcfile_.path(), {},
                            kSystemAppTag);

  // Root can't see user stores.
  ExpectBothKSAdminFetchTag(true, kUserAppID, user_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(false, kUserAppID, user_app_xcfile_.path(), {},
                            kUserAppTag);

  // When running as user, XC path disambiguates.
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            {}, kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID,
                            repeat_app_system_xcfile_.path(), {},
                            kRepeatAppSystemTag);

  // Root can't see user stores, but it doesn't see the mismatching XC path
  // as a reason not to retrieve the entry in the system store, because -- since
  // the user is root -- the user would be able to fix this registration.
  ExpectBothKSAdminFetchTag(true, kRepeatAppID,
                            repeat_app_system_xcfile_.path(), {},
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, repeat_app_user_xcfile_.path(),
                            {}, kRepeatAppSystemTag);
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMismatchElevated) {
  // When running as root, ksadmin only considers the system store, and doesn't
  // consider existence checking path mismatches to stop retrieval.
  ExpectBothKSAdminFetchTag(true, kSystemAppID, no_app_xcfile_.path(), {},
                            kSystemAppTag);
  ExpectBothKSAdminFetchTag(true, kUserAppID, no_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(true, kRepeatAppID, no_app_xcfile_.path(), {},
                            kRepeatAppSystemTag);
  ExpectBothKSAdminFetchTag(true, kNonexistentAppID, no_app_xcfile_.path(), {},
                            {});
}

TEST_F(IntegrationTestKSAdminFourApps, XCPathMismatchUser) {
  // ksadmin knows a user can "fix" the existence checker path in a user
  // registration (and attempting to re-register the app will overwrite that
  // registration), but cannot "fix" (and therefore does not match) a system
  // registration with a different existence checking path.
  ExpectBothKSAdminFetchTag(false, kSystemAppID, no_app_xcfile_.path(), {}, {});
  ExpectBothKSAdminFetchTag(false, kUserAppID, no_app_xcfile_.path(), {},
                            kUserAppTag);
  ExpectBothKSAdminFetchTag(false, kRepeatAppID, no_app_xcfile_.path(), {},
                            kRepeatAppUserTag);
  ExpectBothKSAdminFetchTag(false, kNonexistentAppID, no_app_xcfile_.path(), {},
                            {});
}

TEST_F(IntegrationTestKSAdminFourApps, KSAdminRegisterWithTaggedPkg) {
  base::FilePath tagged_pkg_path =
      test::GetTestFilePath("tagged_pkg").AppendASCII("sample.pkg");

  // Define a temp file path for the brand file, but do not populate it, so
  // "ifneeded" mode will decide a brand file is needed.
  base::ScopedTempFile brand_file;
  ASSERT_TRUE(brand_file.Create());
  ASSERT_TRUE(base::DeleteFile(brand_file.path()));
  // `--brand-value` flag is a fallback; thus, the "WRONG" brand code should
  // be ignored in favor of the "GGLZ" brand tagged onto `sample.pkg`.
  ASSERT_NO_FATAL_FAILURE(ExpectKSAdminRegister(
      UpdaterScope::kUser, kUserAppID, tagged_pkg_path, brand_file.path(),
      "KSBrandID", "WRONG", "ifneeded"));

  base::DictValue expected_app_state;
  expected_app_state.Set("app_id", kUserAppID);
  expected_app_state.Set("brand_code", "GGLZ");

  base::DictValue expected_app_states;
  expected_app_states.Set(kUserAppID, std::move(expected_app_state));
  ASSERT_NO_FATAL_FAILURE(
      user_test_commands_->GetAppStates(expected_app_states));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(brand_file.path(), &content));
  EXPECT_THAT(content, testing::HasSubstr("GGLZ"));
}

TEST_F(IntegrationTestKSAdminFourApps, KSAdminRegisterStompNotCrossUser) {
  base::ScopedTempFile brand_file;
  ASSERT_TRUE(brand_file.Create());

  ASSERT_TRUE(base::WriteFile(brand_file.path(), "OLDCONTENT"));

  ASSERT_NO_FATAL_FAILURE(ExpectKSAdminRegister(
      UpdaterScope::kUser, kRepeatAppID, {}, brand_file.path(), "KSBrandID",
      "STOMPED", "overwrite"));

  std::string content;
  ASSERT_TRUE(base::ReadFileToString(brand_file.path(), &content));
  EXPECT_THAT(content, testing::HasSubstr("STOMPED"));
}

TEST_F(IntegrationTestKSAdminFourApps, CRURegistrationFetchTag) {
  // Direct, unambiguous matches (or nothing matching).
  ExpectCRURegistrationFetchesTag(kSystemAppID, system_app_xcfile_.path(),
                                  kSystemAppTag);
  ExpectCRURegistrationFetchesTag(kUserAppID, user_app_xcfile_.path(),
                                  kUserAppTag);
  ExpectCRURegistrationCannotFetchTag(kNonexistentAppID, no_app_xcfile_.path());

  // Ambiguous app ID, direct XCFile path matches.
  ExpectCRURegistrationFetchesTag(
      kRepeatAppID, repeat_app_system_xcfile_.path(), kRepeatAppSystemTag);
  ExpectCRURegistrationFetchesTag(kRepeatAppID, repeat_app_user_xcfile_.path(),
                                  kRepeatAppUserTag);

  // Non-matching XCFile path can still match user apps, but only user apps.
  ExpectCRURegistrationFetchesTag(kUserAppID, no_app_xcfile_.path(),
                                  kUserAppTag);
  ExpectCRURegistrationFetchesTag(kRepeatAppID, no_app_xcfile_.path(),
                                  kRepeatAppUserTag);
  ExpectCRURegistrationCannotFetchTag(kSystemAppID, no_app_xcfile_.path());
}
#endif  // !defined(ADDRESS_SANITIZER)
#endif  // BUILDFLAG(IS_MAC)

// Windows specific tests.

// Event logging is only implemented on Mac and Windows.
#if BUILDFLAG(IS_MAC)

class EventLoggingIntegrationTest : public IntegrationTest {
 public:
  void SetUp() override {
    IntegrationTest::SetUp();
    ClearPermissionProviderAllowsUsageStats();
  }

  void TearDown() override {
    ClearPermissionProviderAllowsUsageStats();
    IntegrationTest::TearDown();
  }

 protected:
  // Configures whether the provided event logging permission provider enables
  // usage stats.
  void SetPermissionProviderAllowsUsageStats(bool allowed) {
#if BUILDFLAG(IS_MAC)
    test_commands_->SetAppAllowsUsageStats(provider().directory_name, allowed);
#else
    test_commands_->SetAppAllowsUsageStats(provider().app_id, allowed);
#endif
  }

  void ClearPermissionProviderAllowsUsageStats() {
#if BUILDFLAG(IS_MAC)
    test_commands_->ClearAppAllowsUsageStats(provider().directory_name);
#else
    test_commands_->ClearAppAllowsUsageStats(provider().app_id);
#endif
  }

  const EventLoggingPermissionProvider& provider() {
    static base::NoDestructor<EventLoggingPermissionProvider> provider({
        .app_id = "googletest",
#if BUILDFLAG(IS_MAC)
        .directory_name = "googletest",
#endif
    });
    return *provider.get();
  }
};

TEST_F(EventLoggingIntegrationTest, SendsLogs) {
  const base::Version v1("1");

  ScopedServer test_update_server(test_commands_);
  ScopedServer test_event_logging_server(test_commands_);
  EnterTestMode(
      test_update_server.update_url(), test_update_server.crash_upload_url(),
      /*app_logo_url=*/{}, test_event_logging_server.event_logging_url(),
      base::Minutes(5), /*server_keep_alive_time=*/base::Seconds(2),
      /*ceca_connection_timeout=*/base::Seconds(10), provider());

  ExpectInstallEvent(test_update_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_update_server, provider().app_id, /*install_data_index=*/"",
      UpdateService::Priority::kForeground, base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      provider().app_id, /*is_silent_install=*/true, /*tag=*/""));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(
      SetPermissionProviderAllowsUsageStats(/*allowed=*/true));

  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateCheckSequence(test_update_server, provider().app_id,
                                UpdateService::Priority::kForeground, v1, v1));
  test_event_logging_server.ExpectOnce(
      {request::GetPathMatcher(test_event_logging_server.event_logging_path()),
       base::BindRepeating([](const HttpRequest& request) {
         enterprise_companion::telemetry_logger::proto::LogRequest log_request;
         if (!log_request.ParseFromString(request.decoded_content)) {
           ADD_FAILURE() << "Failed to parse log request";
           return false;
         }
         return true;
       })},
      enterprise_companion::telemetry_logger::proto::LogResponse()
          .SerializeAsString(),
      net::HTTP_OK);
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(provider().app_id));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_update_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

TEST_F(EventLoggingIntegrationTest, SkipsLoggingWhenDisallowed) {
  const base::Version v1("1");

  ScopedServer test_update_server(test_commands_);
  ScopedServer test_event_logging_server(test_commands_);
  EnterTestMode(
      test_update_server.update_url(), test_update_server.crash_upload_url(),
      /*app_logo_url=*/{}, test_event_logging_server.event_logging_url(),
      base::Minutes(5), /*server_keep_alive_time=*/base::Seconds(2),
      /*ceca_connection_timeout=*/base::Seconds(10), provider());

  ExpectInstallEvent(test_update_server, kUpdaterAppId);
  ASSERT_NO_FATAL_FAILURE(ExpectInstallSequence(
      test_update_server, provider().app_id, /*install_data_index=*/"",
      UpdateService::Priority::kForeground, base::Version({0, 0, 0, 0}), v1));
  ASSERT_NO_FATAL_FAILURE(InstallUpdaterAndApp(
      provider().app_id, /*is_silent_install=*/true, /*tag=*/""));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(
      SetPermissionProviderAllowsUsageStats(/*allowed=*/false));

  ASSERT_NO_FATAL_FAILURE(
      ExpectUpdateCheckSequence(test_update_server, provider().app_id,
                                UpdateService::Priority::kForeground, v1, v1));
  ASSERT_NO_FATAL_FAILURE(CheckForUpdate(provider().app_id));
  ASSERT_TRUE(WaitForUpdaterExit());

  ASSERT_NO_FATAL_FAILURE(ExpectUninstallPing(test_update_server));
  ASSERT_NO_FATAL_FAILURE(Uninstall());
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace updater::test
