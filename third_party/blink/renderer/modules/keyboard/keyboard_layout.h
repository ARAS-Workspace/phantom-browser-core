// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_property.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"

namespace blink {

class DOMException;
class ExceptionState;
class KeyboardLayoutMap;

class KeyboardLayout final : public GarbageCollected<KeyboardLayout>,
                             public ExecutionContextClient {
 public:
  explicit KeyboardLayout(ExecutionContext*);

  KeyboardLayout(const KeyboardLayout&) = delete;
  KeyboardLayout& operator=(const KeyboardLayout&) = delete;

  ~KeyboardLayout() = default;

  ScriptPromise<KeyboardLayoutMap> GetKeyboardLayoutMap(ScriptState*,
                                                        ExceptionState&);

  void Trace(Visitor*) const override;

 private:
  // Returns true if the local frame is attached to the renderer.
  bool IsLocalFrameAttached();

  using LayoutMapProperty =
      ScriptPromiseProperty<KeyboardLayoutMap, DOMException>;
  Member<LayoutMapProperty> layout_map_property_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_KEYBOARD_KEYBOARD_LAYOUT_H_
