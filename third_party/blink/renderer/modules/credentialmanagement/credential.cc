// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

namespace blink {

namespace {
constexpr char kIdentityCredentialType[] = "identity";
constexpr char kOtpCredentialType[] = "otp";
}  // namespace

Credential::~Credential() = default;

Credential::Credential(const String& id, const String& type)
    : id_(id), type_(type) {
  DCHECK(!id_.empty() || type == kIdentityCredentialType ||
         type == kOtpCredentialType);
  DCHECK(!type_.empty());
}

// static
ScriptPromise<IDLBoolean> Credential::isConditionalMediationAvailable(
    ScriptState* script_state) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(script_state);
  auto promise = resolver->Promise();
  resolver->Resolve(false);
  return promise;
}

void Credential::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
