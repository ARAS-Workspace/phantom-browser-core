// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"
#include "components/webapps/isolated_web_apps/types/isolated_web_app_external_install_options.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"

class Profile;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace web_app {

class UpdateManifest;
class WebAppProvider;

enum class IwaInstallerResultType {
  kSuccess,
  kErrorCantCreateTempFile,
  kErrorUpdateManifestDownloadFailed,
  kErrorUpdateManifestParsingFailed,
  kErrorWebBundleUrlCantBeDetermined,
  kErrorCantDownloadWebBundle,
  kErrorCantInstallFromWebBundle,
  kErrorManagedGuestSessionInstallDisabled,
  kErrorAppNotInAllowlist
};

class IwaInstallerResult {
 public:
  using Type = IwaInstallerResultType;

  explicit IwaInstallerResult(Type type, std::string message = "");

  [[nodiscard]] base::DictValue ToDebugValue() const;

  [[nodiscard]] Type type() const { return type_; }

  [[nodiscard]] std::string_view message() const { return message_; }

 private:
  Type type_;
  std::string message_;
};

// This class installs an IWA based on a policy configuration.
class IwaInstaller {
 public:
  using Result = IwaInstallerResult;
  using ResultCallback = base::OnceCallback<void(Result)>;

  enum class InstallSourceType {
    kPolicy = 0,  // App listed in IsolatedWebAppInstallForceList policy.
    kKiosk = 1,   // Kiosk app defined via DeviceLocalAccount policy.
  };

  IwaInstaller(IsolatedWebAppExternalInstallOptions install_options,
               InstallSourceType install_source_type,
               Profile* profile,
               base::ListValue& log,
               ResultCallback callback);
  ~IwaInstaller();

  // Starts installing the IWA in session (user, MGS or kiosk).
  void Start();

  IwaInstaller(const IwaInstaller&) = delete;
  IwaInstaller& operator=(const IwaInstaller&) = delete;

 private:

  void CreateTempFile(base::OnceClosure next_step_callback);
  void OnTempFileCreated(base::OnceClosure next_step_callback,
                         ScopedTempWebBundleFile bundle);

  void InstallFromInternet();

  // Downloading of the update manifest of the current app.
  void DownloadUpdateManifest(
      base::OnceCallback<void(GURL, IwaVersion)> next_step_callback);

  // Callback when the update manifest has been downloaded and parsed.
  void OnUpdateManifestParsed(
      base::OnceCallback<void(GURL, IwaVersion)> next_step_callback,
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          fetch_result);

  // Downloading of the Signed Web Bundle.
  void DownloadWebBundle(
      base::OnceCallback<void(IwaVersion)> next_step_callback,
      GURL web_bundle_url,
      IwaVersion expected_version);
  void OnWebBundleDownloaded(base::OnceClosure next_step_callback,
                             int32_t net_error);

  // Installing of the IWA using the downloaded Signed Web Bundle.
  void RunInstallFromInternetCommand(IwaVersion expected_version);
  void OnIwaInstalledFromInternet(
      IwaVersion installed_version,
      base::expected<InstallIsolatedWebAppCommandSuccess,
                     InstallIsolatedWebAppCommandError> result);

  void Finish(Result result);

  IsolatedWebAppExternalInstallOptions install_options_;
  InstallSourceType install_source_type_;

  const raw_ptr<Profile> profile_;
  raw_ref<base::ListValue> log_;
  ResultCallback callback_;

  ScopedTempWebBundleFile bundle_;

  std::unique_ptr<UpdateManifestFetcher> update_manifest_fetcher_;
  std::unique_ptr<IsolatedWebAppDownloader> bundle_downloader_;

  base::WeakPtrFactory<IwaInstaller> weak_factory_{this};
};

std::ostream& operator<<(std::ostream& os,
                         IwaInstallerResultType install_result_type);

}  // namespace web_app
#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_POLICY_ISOLATED_WEB_APP_INSTALLER_H_
