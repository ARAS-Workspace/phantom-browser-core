// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/keyboard/keyboard_layout.h"

#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/keyboard/keyboard_layout_map.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

constexpr char kKeyboardMapFrameDetachedErrorMsg[] =
    "Current frame is detached.";

constexpr struct {
  const char* code;
  const char* key;
} kLayout[] = {
    {"Backquote", "`"},    {"Digit1", "1"},      {"Digit2", "2"},
    {"Digit3", "3"},       {"Digit4", "4"},      {"Digit5", "5"},
    {"Digit6", "6"},       {"Digit7", "7"},      {"Digit8", "8"},
    {"Digit9", "9"},       {"Digit0", "0"},      {"Minus", "-"},
    {"Equal", "="},        {"KeyQ", "q"},        {"KeyW", "w"},
    {"KeyE", "e"},         {"KeyR", "r"},        {"KeyT", "t"},
    {"KeyY", "y"},         {"KeyU", "u"},        {"KeyI", "i"},
    {"KeyO", "o"},         {"KeyP", "p"},        {"BracketLeft", "["},
    {"BracketRight", "]"}, {"Backslash", "\\"},  {"KeyA", "a"},
    {"KeyS", "s"},         {"KeyD", "d"},        {"KeyF", "f"},
    {"KeyG", "g"},         {"KeyH", "h"},        {"KeyJ", "j"},
    {"KeyK", "k"},         {"KeyL", "l"},        {"Semicolon", ";"},
    {"Quote", "'"},        {"KeyZ", "z"},        {"KeyX", "x"},
    {"KeyC", "c"},         {"KeyV", "v"},        {"KeyB", "b"},
    {"KeyN", "n"},         {"KeyM", "m"},        {"Comma", ","},
    {"Period", "."},       {"Slash", "/"},
};

HashMap<String, String> FixedLayoutMap() {
  HashMap<String, String> map;
  for (const auto& entry : kLayout) {
    map.insert(String(entry.code), String(entry.key));
  }
  return map;
}

}  // namespace

KeyboardLayout::KeyboardLayout(ExecutionContext* context)
    : ExecutionContextClient(context) {}

ScriptPromise<KeyboardLayoutMap> KeyboardLayout::GetKeyboardLayoutMap(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  DCHECK(script_state);

  if (!IsLocalFrameAttached()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      kKeyboardMapFrameDetachedErrorMsg);
    return EmptyPromise();
  }

  if (!layout_map_property_) {
    layout_map_property_ = MakeGarbageCollected<LayoutMapProperty>(
        ExecutionContext::From(script_state));
  }

  auto promise = layout_map_property_->Promise(script_state->World());
  layout_map_property_->Resolve(
      MakeGarbageCollected<KeyboardLayoutMap>(FixedLayoutMap()));
  layout_map_property_ = nullptr;
  return promise;
}

bool KeyboardLayout::IsLocalFrameAttached() {
  return DomWindow();
}

void KeyboardLayout::Trace(Visitor* visitor) const {
  visitor->Trace(layout_map_property_);
  ExecutionContextClient::Trace(visitor);
}

}  // namespace blink
