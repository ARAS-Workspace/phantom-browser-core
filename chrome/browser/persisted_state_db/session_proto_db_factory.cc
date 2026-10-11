// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/session_proto_db/persisted_state_db_content.pb.h"

SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
GetPersistedStateSessionProtoDBFactory() {
  static base::NoDestructor<
      SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>>
      instance;
  return instance.get();
}

template <>
SessionProtoDBFactory<persisted_state_db::PersistedStateContentProto>*
SessionProtoDBFactory<
    persisted_state_db::PersistedStateContentProto>::GetInstance() {
  return GetPersistedStateSessionProtoDBFactory();
}
