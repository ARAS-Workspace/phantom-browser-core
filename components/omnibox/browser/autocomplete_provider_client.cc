// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/autocomplete_provider_client.h"

#include "base/notreached.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

history_clusters::HistoryClustersService*
AutocompleteProviderClient::GetHistoryClustersService() {
  return nullptr;
}

history_embeddings::HistoryEmbeddingsSearch*
AutocompleteProviderClient::GetHistoryEmbeddingsSearch() {
  return nullptr;
}

GeolocationHeaderService*
AutocompleteProviderClient::GetGeolocationHeaderService() const {
  return nullptr;
}

DocumentSuggestionsService*
AutocompleteProviderClient::GetDocumentSuggestionsService() const {
  return nullptr;
}

bool AutocompleteProviderClient::AllowDeletingBrowserHistory() const {
  return true;
}

std::string AutocompleteProviderClient::ProfileUserName() const {
  return "";
}

bool AutocompleteProviderClient::IsIncognitoModeAvailable() const {
  return true;
}

bool AutocompleteProviderClient::IsSharingHubAvailable() const {
  return false;
}

bool AutocompleteProviderClient::IsHistoryEmbeddingsEnabled() const {
  return false;
}

bool AutocompleteProviderClient::IsHistoryEmbeddingsSettingVisible() const {
  return false;
}

std::optional<bool> AutocompleteProviderClient::IsPagePaywalled() const {
  return std::nullopt;
}

bool AutocompleteProviderClient::in_background_state() const {
  return false;
}

bool AutocompleteProviderClient::IsGeminiStarterPackEnabled() const {
  return OmniboxFieldTrial::IsStarterPackExpansionEnabled();
}

base::WeakPtr<AutocompleteProviderClient>
AutocompleteProviderClient::GetWeakPtr() {
  return nullptr;
}

bool AutocompleteProviderClient::IsWebUiNtpEnabledForDesktopAndroid() const {
  return false;
}
