// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
#define COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_

#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

// A namespace for recording spell-check related histograms.
// This namespace encapsulates histogram names and metrics API for the renderer
// side of the spell checker.
namespace spellcheck_renderer_metrics {

// The length of text checked via async checking.
void RecordAsyncCheckedTextLength(int length);

// The length of text checked by spellCheck. No replacement suggestions were
// requested.
void RecordCheckedTextLengthNoSuggestions(int length);

// The length of text checked by spellCheck. Replacement suggestions were
// requested.
void RecordCheckedTextLengthWithSuggestions(int length);

}  // namespace spellcheck_renderer_metrics

#endif  // COMPONENTS_SPELLCHECK_RENDERER_SPELLCHECK_RENDERER_METRICS_H_
