// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_image_service/image_service_impl.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "components/omnibox/browser/autocomplete_scheme_classifier.h"
#include "components/omnibox/browser/remote_suggestions_service.h"
#include "components/omnibox/browser/search_suggestion_parser.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/salient_image_metadata.pb.h"
#include "components/page_image_service/image_service_consent_helper.h"
#include "components/page_image_service/metrics_util.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace page_image_service {

namespace {

// Fulfills all `callbacks` with `result`.
}  // namespace

// A one-time use object that uses Suggest to get an image URL corresponding
// to `search_query` and `entity_id`. This is a hacky temporary implementation,
// ideally this should be replaced by persisted Suggest-provided entities.
// TODO(tommycli): Move this to its own separate file with unit tests.
class ImageServiceImpl::SuggestEntityImageURLFetcher {
 public:
  SuggestEntityImageURLFetcher(
      const AutocompleteSchemeClassifier& autocomplete_scheme_classifier,
      mojom::ClientId client_id,
      const std::u16string& search_query,
      const std::string& entity_id)
      : autocomplete_scheme_classifier_(autocomplete_scheme_classifier),
        client_id_(client_id),
        search_query_(base::i18n::ToLower(search_query)),
        entity_id_(entity_id) {}
  SuggestEntityImageURLFetcher(const SuggestEntityImageURLFetcher&) = delete;

  // `callback` is called with the result.
  void Start(const TemplateURL* template_url,
             const SearchTermsData& search_terms_data,
             RemoteSuggestionsService* remote_suggestions_service,
             base::OnceCallback<void(const GURL&)> callback) {
    CHECK(template_url);
    CHECK(remote_suggestions_service);
    CHECK(!callback_);
    callback_ = std::move(callback);

    TemplateURLRef::SearchTermsArgs search_terms_args;
    search_terms_args.page_classification =
        metrics::OmniboxEventProto::JOURNEYS;
    search_terms_args.search_terms = search_query_;

    // ImageServiceFactory does not create a service instance for OTR profiles.
    loader_ = remote_suggestions_service->StartSuggestionsRequest(
        RemoteRequestType::kImages, /*is_off_the_record=*/false, template_url,
        search_terms_args, search_terms_data,
        base::BindOnce(&SuggestEntityImageURLFetcher::OnURLLoadComplete,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void OnURLLoadComplete(const network::SimpleURLLoader* source,
                         const int response_code,
                         std::optional<std::string> response_body) {
    DCHECK_EQ(loader_.get(), source);
    if (response_code != 200) {
      UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                       PageImageServiceResult::kResponseMissing,
                                       client_id_);
      return std::move(callback_).Run(GURL());
    }

    std::string response_json = SearchSuggestionParser::ExtractJsonData(
        source, std::move(response_body));
    if (response_json.empty()) {
      UmaHistogramEnumerationForClient(
          kBackendSuggestResultHistogramName,
          PageImageServiceResult::kResponseMalformed, client_id_);
      return std::move(callback_).Run(GURL());
    }

    auto response_data =
        SearchSuggestionParser::DeserializeJsonData(response_json);
    if (!response_data) {
      UmaHistogramEnumerationForClient(
          kBackendSuggestResultHistogramName,
          PageImageServiceResult::kResponseMalformed, client_id_);
      return std::move(callback_).Run(GURL());
    }

    AutocompleteInput input(search_query_, metrics::OmniboxEventProto::JOURNEYS,
                            *autocomplete_scheme_classifier_);
    SearchSuggestionParser::Results results;
    if (!SearchSuggestionParser::ParseSuggestResults(
            *response_data, input, *autocomplete_scheme_classifier_,
            /*default_result_relevance=*/100,
            /*is_keyword_result=*/false, &results)) {
      UmaHistogramEnumerationForClient(
          kBackendSuggestResultHistogramName,
          PageImageServiceResult::kResponseMalformed, client_id_);
      return std::move(callback_).Run(GURL());
    }

    for (const auto& result : results.suggest_results) {
      // TODO(tommycli): `entity_id_` is not used yet, because it's always
      // empty right now.
      GURL url(result.entity_info().image_url());
      if (url.is_valid() &&
          base::i18n::ToLower(result.match_contents()) == search_query_) {
        UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                         PageImageServiceResult::kSuccess,
                                         client_id_);
        return std::move(callback_).Run(std::move(url));
      }
    }

    // If we didn't find any matching images, still notify the caller.
    if (!callback_.is_null()) {
      UmaHistogramEnumerationForClient(kBackendSuggestResultHistogramName,
                                       PageImageServiceResult::kNoImage,
                                       client_id_);
      std::move(callback_).Run(GURL());
    }
  }

  // Embedder-specific logic on how to classify schemes.
  const raw_ref<const AutocompleteSchemeClassifier>
      autocomplete_scheme_classifier_;

  // The id of the UI requesting the image.
  mojom::ClientId client_id_;

  // The search query and entity ID we are searching for.
  const std::u16string search_query_;
  const std::string entity_id_;

  // The result callback to be called once we get the answer.
  base::OnceCallback<void(const GURL&)> callback_;

  // The URL loader used to get the suggestions.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<SuggestEntityImageURLFetcher> weak_factory_{this};
};

ImageServiceImpl::ImageServiceImpl(
    TemplateURLService* template_url_service,
    RemoteSuggestionsService* remote_suggestions_service,
    optimization_guide::OptimizationGuideDecider* opt_guide,
    syncer::SyncService* sync_service,
    std::unique_ptr<AutocompleteSchemeClassifier>
        autocomplete_scheme_classifier)
    : template_url_service_(template_url_service),
      remote_suggestions_service_(remote_suggestions_service),
      history_consent_helper_(std::make_unique<ImageServiceConsentHelper>(
          sync_service,
          syncer::DataType::HISTORY_DELETE_DIRECTIVES)),
      bookmarks_consent_helper_(std::make_unique<ImageServiceConsentHelper>(
          sync_service,
          syncer::DataType::BOOKMARKS)),
      autocomplete_scheme_classifier_(
          std::move(autocomplete_scheme_classifier)) {}

ImageServiceImpl::~ImageServiceImpl() = default;

base::WeakPtr<ImageService> ImageServiceImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void ImageServiceImpl::FetchImageFor(mojom::ClientId client_id,
                                     const GURL& page_url,
                                     const mojom::Options& options,
                                     ResultCallback callback) {
  GetConsentToFetchImage(
      client_id,
      base::BindOnce(&ImageServiceImpl::OnConsentResult, weak_factory_.GetWeakPtr(),
                     client_id, page_url, options, std::move(callback)));
}

void ImageServiceImpl::GetConsentToFetchImage(
    mojom::ClientId client_id,
    base::OnceCallback<void(PageImageServiceConsentStatus)> callback) {
  switch (client_id) {
    case mojom::ClientId::Journeys:
    case mojom::ClientId::JourneysSidePanel:
    case mojom::ClientId::HistoryEmbeddings:
    case mojom::ClientId::NtpQuests:
    case mojom::ClientId::NtpTabResumption: {
      return history_consent_helper_->EnqueueRequest(std::move(callback),
                                                     client_id);
    }
    case mojom::ClientId::NtpRealbox:
      // TODO(b/244507194): Figure out consent story for NTP realbox case.
      return std::move(callback).Run(PageImageServiceConsentStatus::kFailure);
    case mojom::ClientId::Bookmarks: {
      return bookmarks_consent_helper_->EnqueueRequest(std::move(callback),
                                                       client_id);
    }
  }
}

void ImageServiceImpl::OnConsentResult(mojom::ClientId client_id,
                                   const GURL& page_url,
                                   const mojom::Options& options,
                                   ResultCallback callback,
                                   PageImageServiceConsentStatus status) {
  base::UmaHistogramEnumeration(kConsentStatusHistogramName, status);
  base::UmaHistogramEnumeration(std::string(kConsentStatusHistogramName) + "." +
                                    ClientIdToString(client_id),
                                status);

  if (status != PageImageServiceConsentStatus::kSuccess) {
    return std::move(callback).Run(GURL());
  }

  if (options.suggest_images && template_url_service_ &&
      remote_suggestions_service_) {
    auto search_metadata =
        template_url_service_->ExtractSearchMetadata(page_url);
    // Fetch entity-keyed images for Google SRP visits only, because only
    // Google SRP visits can expect to have a reasonable entity from Google
    // Suggest.
    if (search_metadata && search_metadata->template_url &&
        search_metadata->template_url->GetEngineType(
            template_url_service_->search_terms_data()) ==
            SEARCH_ENGINE_GOOGLE) {
      UmaHistogramEnumerationForClient(
          kBackendHistogramName, PageImageServiceBackend::kSuggest, client_id);
      return FetchSuggestImage(search_metadata->template_url,
                               template_url_service_->search_terms_data(),
                               client_id,
                               /*search_query=*/search_metadata->search_terms,
                               /*entity_id=*/"", std::move(callback));
    }
  }

  UmaHistogramEnumerationForClient(kBackendHistogramName,
                                   PageImageServiceBackend::kNoValidBackend,
                                   client_id);
  std::move(callback).Run(GURL());
}

void ImageServiceImpl::FetchSuggestImage(const TemplateURL* template_url,
                                     const SearchTermsData& search_terms_data,
                                     mojom::ClientId client_id,
                                     const std::u16string& search_query,
                                     const std::string& entity_id,
                                     ResultCallback callback) {
  auto fetcher = std::make_unique<SuggestEntityImageURLFetcher>(
      *autocomplete_scheme_classifier_, client_id, search_query, entity_id);

  // Use a raw pointer temporary so we can give ownership of the unique_ptr to
  // the callback and have a well defined SuggestEntityImageURLFetcher lifetime.
  auto* fetcher_raw_ptr = fetcher.get();
  fetcher_raw_ptr->Start(
      template_url, search_terms_data, remote_suggestions_service_,
      base::BindOnce(&ImageServiceImpl::OnSuggestImageFetched,
                     weak_factory_.GetWeakPtr(), std::move(fetcher),
                     std::move(callback)));
}

void ImageServiceImpl::OnSuggestImageFetched(
    std::unique_ptr<SuggestEntityImageURLFetcher> fetcher,
    ResultCallback callback,
    const GURL& image_url) {
  std::move(callback).Run(image_url);

  // `fetcher` is owned by this method and will be deleted now.
}

}  // namespace page_image_service
