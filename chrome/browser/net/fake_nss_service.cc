// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/fake_nss_service.h"

#include <memory>

#include "chrome/browser/net/nss_service_factory.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "nss_service.h"

namespace {
net::NSSCertDatabase* NssGetterForIOThread(
    net::NSSCertDatabase* result,
    base::OnceCallback<void(net::NSSCertDatabase*)>) {
  // The check is here because the real NSS getter must also be run on the IO
  // thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return result;
}

std::unique_ptr<KeyedService> CreateService(bool enable_system_slot,
                                            content::BrowserContext* context) {
  return std::make_unique<FakeNssService>(context, enable_system_slot);
}

}  // namespace

// static
FakeNssService* FakeNssService::InitializeForBrowserContext(
    content::BrowserContext* context) {
  KeyedService* service =
      NssServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating(&CreateService, false));
  return static_cast<FakeNssService*>(service);
}

FakeNssService::FakeNssService(content::BrowserContext* context,
                               bool enable_system_slot)
    : NssService(context) {
  public_slot_ = std::make_unique<crypto::ScopedTestNSSDB>();
  nss_cert_database_ = std::make_unique<net::NSSCertDatabase>(
      crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_slot_->slot())),
      crypto::ScopedPK11Slot(PK11_ReferenceSlot(public_slot_->slot())));
}

FakeNssService::~FakeNssService() {
  content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                 std::move(nss_cert_database_));
}

NssCertDatabaseGetter FakeNssService::CreateNSSCertDatabaseGetterForIOThread() {
  return base::BindOnce(&NssGetterForIOThread, nss_cert_database_.get());
}

PK11SlotInfo* FakeNssService::GetPublicSlot() const {
  return public_slot_->slot();
}
