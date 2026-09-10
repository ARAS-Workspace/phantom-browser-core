// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/spellcheck/renderer/spellcheck_renderer_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/spellcheck/spellcheck_buildflags.h"

namespace spellcheck_renderer_metrics {

void RecordAsyncCheckedTextLength(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.async", length);
}

void RecordCheckedTextLengthNoSuggestions(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check", length);
}

void RecordCheckedTextLengthWithSuggestions(int length) {
  UMA_HISTOGRAM_COUNTS_1M("SpellCheck.api.check.suggestions", length);
}

}  // namespace spellcheck_renderer_metrics
