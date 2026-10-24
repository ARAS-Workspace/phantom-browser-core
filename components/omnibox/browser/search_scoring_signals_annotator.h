// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_SEARCH_SCORING_SIGNALS_ANNOTATOR_H_
#define COMPONENTS_OMNIBOX_BROWSER_SEARCH_SCORING_SIGNALS_ANNOTATOR_H_

#include <string>

struct AutocompleteMatch;

// Annotates autocomplete Search suggestion candidates with scoring signals
// derived from suggestions obtained from the remote Suggest service, including:
// `search_suggest_relevance`, `is_search_suggest_entity`.
class SearchScoringSignalsAnnotator {
 public:
  SearchScoringSignalsAnnotator() = delete;
  SearchScoringSignalsAnnotator(const SearchScoringSignalsAnnotator&) = delete;
  SearchScoringSignalsAnnotator& operator=(
      const SearchScoringSignalsAnnotator&) = delete;

  // Updates various scoring signals whose values are determined by the
  // autocomplete match type (and potentially the user's input text).
  static void UpdateMatchTypeScoringSignals(AutocompleteMatch& match,
                                            const std::u16string& input_text);
};

#endif  // COMPONENTS_OMNIBOX_BROWSER_SEARCH_SCORING_SIGNALS_ANNOTATOR_H_
