// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cdm_document_service_impl.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/security_principal.h"
#include "content/public/browser/web_contents.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
#include "chrome/browser/media/cdm_storage_id.h"
#include "chrome/browser/media/media_storage_id_salt.h"
#include "content/public/browser/render_process_host.h"
#endif

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/media/platform_verification_chromeos.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#endif

namespace {

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
// Only support version 1 of Storage Id. However, the "latest" version can also
// be requested.
const uint32_t kRequestLatestStorageIdVersion = 0;
const uint32_t kCurrentStorageIdVersion = 1;

std::vector<uint8_t> GetStorageIdSaltFromProfile(
    content::RenderFrameHost* rfh) {
  DCHECK(rfh);
  Profile* profile =
      Profile::FromBrowserContext(rfh->GetProcess()->GetBrowserContext());
  return MediaStorageIdSalt::GetSalt(profile->GetPrefs());
}

#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

}  // namespace

// static
void CdmDocumentServiceImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver) {
  DVLOG(2) << __func__;
  CHECK(render_frame_host);

  // PlatformVerificationFlow and the pref service requires to be run/accessed
  // on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new CdmDocumentServiceImpl(*render_frame_host, std::move(receiver));
}

CdmDocumentServiceImpl::CdmDocumentServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<media::mojom::CdmDocumentService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

CdmDocumentServiceImpl::~CdmDocumentServiceImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void CdmDocumentServiceImpl::ChallengePlatform(
    const std::string& service_id,
    const std::string& challenge,
    ChallengePlatformCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40499115). This should be commented out at the mojom
  // level so that it's only available for ChromeOS.

#if BUILDFLAG(IS_CHROMEOS)
  bool success = platform_verification::PerformBrowserChecks(
      render_frame_host().GetMainFrame());
  if (!success) {
    std::move(callback).Run(false, std::string(), std::string(), std::string());
    return;
  }

  if (!platform_verification_flow_)
    platform_verification_flow_ =
        base::MakeRefCounted<ash::attestation::PlatformVerificationFlow>();

  platform_verification_flow_->ChallengePlatformKey(
      content::WebContents::FromRenderFrameHost(&render_frame_host()),
      service_id, challenge,
      base::BindOnce(&CdmDocumentServiceImpl::OnPlatformChallenged,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#else
  // Not supported, so return failure.
  std::move(callback).Run(false, std::string(), std::string(), std::string());
#endif  // BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS)
void CdmDocumentServiceImpl::OnPlatformChallenged(
    ChallengePlatformCallback callback,
    PlatformVerificationResult result,
    const std::string& signed_data,
    const std::string& signature,
    const std::string& platform_key_certificate) {
  DVLOG(2) << __func__ << ": " << result;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != ash::attestation::PlatformVerificationFlow::SUCCESS) {
    DCHECK(signed_data.empty());
    DCHECK(signature.empty());
    DCHECK(platform_key_certificate.empty());
    LOG(ERROR) << "Platform verification failed.";
    std::move(callback).Run(false, "", "", "");
    return;
  }

  DCHECK(!signed_data.empty());
  DCHECK(!signature.empty());
  DCHECK(!platform_key_certificate.empty());
  std::move(callback).Run(true, signed_data, signature,
                          platform_key_certificate);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void CdmDocumentServiceImpl::GetStorageId(uint32_t version,
                                          GetStorageIdCallback callback) {
  DVLOG(2) << __func__ << " version: " << version;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(crbug.com/40499115). This should be commented out at the mojom
  // level so that it's only available if Storage Id is available.

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
  // Check that the request is for a supported version.
  if (version == kCurrentStorageIdVersion ||
      version == kRequestLatestStorageIdVersion) {
    ComputeStorageId(
        GetStorageIdSaltFromProfile(&render_frame_host()), origin(),
        base::BindOnce(&CdmDocumentServiceImpl::OnStorageIdResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

  // Version not supported, so no Storage Id to return.
  DVLOG(2) << __func__ << " not supported";
  std::move(callback).Run(version, std::vector<uint8_t>());
}

#if BUILDFLAG(ENABLE_CDM_STORAGE_ID)
void CdmDocumentServiceImpl::OnStorageIdResponse(
    GetStorageIdCallback callback,
    const std::vector<uint8_t>& storage_id) {
  DVLOG(2) << __func__ << " version: " << kCurrentStorageIdVersion
           << ", size: " << storage_id.size();

  std::move(callback).Run(kCurrentStorageIdVersion, storage_id);
}
#endif  // BUILDFLAG(ENABLE_CDM_STORAGE_ID)

#if BUILDFLAG(IS_CHROMEOS)
void CdmDocumentServiceImpl::IsVerifiedAccessEnabled(
    IsVerifiedAccessEnabledCallback callback) {
  // If we are in guest/incognito mode, then verified access is effectively
  // disabled.
  Profile* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());
  if (profile->IsOffTheRecord() || profile->IsGuestSession()) {
    std::move(callback).Run(false);
    return;
  }

  bool enabled_for_device = false;
  ash::CrosSettings::Get()->GetBoolean(
      ash::kAttestationForContentProtectionEnabled, &enabled_for_device);
  std::move(callback).Run(enabled_for_device);
}
#endif  // BUILDFLAG(IS_CHROMEOS)
