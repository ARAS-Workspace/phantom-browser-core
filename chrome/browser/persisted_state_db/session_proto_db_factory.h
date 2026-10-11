// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSISTED_STATE_DB_SESSION_PROTO_DB_FACTORY_H_
#define CHROME_BROWSER_PERSISTED_STATE_DB_SESSION_PROTO_DB_FACTORY_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/session_proto_db/session_proto_db.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"

namespace {
const char kPersistedStateDBFolder[] = "persisted_state_db";
}  // namespace

SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateSessionProtoDBFactory();

// Factory to create a ProtoDB per browsing session (BrowserContext) and per
// proto. Incognito is currently not supported and the factory will return
// nullptr for an incognito profile.
template <typename T>
class SessionProtoDBFactory : public ProfileKeyedServiceFactory {
 public:
  // Acquire instance of SessionProtoDBFactory.
  static SessionProtoDBFactory<T>* GetInstance();

  // Acquire ProtoDB - there is one per BrowserContext.
  static SessionProtoDB<T>* GetForProfile(content::BrowserContext* context);

  // Call the parent Disassociate which is a protected method.
  void Disassociate(content::BrowserContext* context) {
    BrowserContextKeyedServiceFactory::Disassociate(context);
  }

 private:
  friend class base::NoDestructor<SessionProtoDBFactory<T>>;

  SessionProtoDBFactory();
  ~SessionProtoDBFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

// static
template <typename T>
SessionProtoDB<T>* SessionProtoDBFactory<T>::GetForProfile(
    content::BrowserContext* context) {
  // Incognito is currently not supported
  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  return static_cast<SessionProtoDB<T>*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

template <typename T>
SessionProtoDBFactory<T>::SessionProtoDBFactory()
    : ProfileKeyedServiceFactory(
          "SessionProtoDBFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

template <typename T>
SessionProtoDBFactory<T>::~SessionProtoDBFactory() = default;

template <typename T>
std::unique_ptr<KeyedService>
SessionProtoDBFactory<T>::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  leveldb_proto::ProtoDatabaseProvider* proto_database_provider =
      context->GetDefaultStoragePartition()->GetProtoDatabaseProvider();

  // The following will become a proto -> dir and proto ->
  // leveldb_proto::ProtoDbType mapping as more protos are added.
  if (std::is_base_of<persisted_state_db::PersistedStateContentProto,
                      T>::value) {
    return std::make_unique<SessionProtoDB<T>>(
        proto_database_provider,
        context->GetPath().AppendASCII(kPersistedStateDBFolder),
        leveldb_proto::ProtoDbType::PERSISTED_STATE_DATABASE,
        content::GetUIThreadTaskRunner({}));
  } else {
    // Must add in leveldb_proto::ProtoDbType and database directory folder for
    // new protos.
    DCHECK(false) << "Provided template is not supported. To support add "
                     "unique folder in the above proto -> folder name mapping. "
                     "This check could also fail because the template is not "
                     "supported on current platform.";
  }
}

#endif  // CHROME_BROWSER_PERSISTED_STATE_DB_SESSION_PROTO_DB_FACTORY_H_
