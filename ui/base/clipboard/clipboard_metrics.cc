// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/clipboard_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_tokenizer.h"

namespace ui {

void RecordRead(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Read", metric);
}

void RecordWrite(ClipboardFormatMetric metric) {
  base::UmaHistogramEnumeration("Clipboard.Write", metric);
}

void RecordWriteTextSizeMetrics(std::u16string_view text) {
  int words = 0;
  base::StringView16Tokenizer tokenizer(
      text, u"", base::StringView16Tokenizer::WhitespacePolicy::kSkipOver);
  while (tokenizer.GetNext()) {
    ++words;
  }
  base::UmaHistogramCounts100000("Clipboard.Write.Text.WordCount", words);
  base::UmaHistogramCounts100000("Clipboard.Write.Text.CharacterCount",
                                 text.size());
}

}  // namespace ui
