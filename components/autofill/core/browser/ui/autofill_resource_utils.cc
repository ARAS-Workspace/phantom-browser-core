// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_resource_utils.h"

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/grit/components_scaled_resources.h"

namespace autofill {

namespace {

// Used in the IDS_ space as a placeholder for resources that don't exist.
constexpr int kResourceNotFoundId = 0;

bool ShouldUseNewFopDisplay() {
  return true;
}

constexpr auto kOldDataResources = base::MakeFixedFlatMap<Suggestion::Icon,
                                                          int>({
    {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_METADATA_CC_AMEX_OLD},
    {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_METADATA_CC_DINERS_OLD},
    {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_METADATA_CC_DISCOVER_OLD},
    {Suggestion::Icon::kCardElo, IDR_AUTOFILL_METADATA_CC_ELO_OLD},
    {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_METADATA_CC_GENERIC_OLD},
    {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_METADATA_CC_JCB_OLD},
    {Suggestion::Icon::kCardMasterCard,
     IDR_AUTOFILL_METADATA_CC_MASTERCARD_OLD},
    {Suggestion::Icon::kCardMir, IDR_AUTOFILL_METADATA_CC_MIR_OLD},
    {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_METADATA_CC_TROY_OLD},
    {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_METADATA_CC_UNIONPAY_OLD},
    {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_METADATA_CC_VERVE_OLD},
    {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_METADATA_CC_VISA_OLD},
    {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN_OLD},
    {Suggestion::Icon::kBnplGeneric, IDR_AUTOFILL_METADATA_BNPL_GENERIC_OLD},
    {Suggestion::Icon::kBnplAffirm, IDR_AUTOFILL_METADATA_AFFIRM},
    {Suggestion::Icon::kBnplAfterpay, IDR_AUTOFILL_METADATA_AFTERPAY},
    {Suggestion::Icon::kBnplKlarna, IDR_AUTOFILL_METADATA_KLARNA},
    {Suggestion::Icon::kBnplZip, IDR_AUTOFILL_METADATA_ZIP},
});

constexpr auto kDataResources = base::MakeFixedFlatMap<Suggestion::Icon, int>({
    {Suggestion::Icon::kCardAmericanExpress, IDR_AUTOFILL_METADATA_CC_AMEX},
    {Suggestion::Icon::kCardDiners, IDR_AUTOFILL_METADATA_CC_DINERS},
    {Suggestion::Icon::kCardDiscover, IDR_AUTOFILL_METADATA_CC_DISCOVER},
    {Suggestion::Icon::kCardElo, IDR_AUTOFILL_METADATA_CC_ELO},
    {Suggestion::Icon::kCardGeneric, IDR_AUTOFILL_METADATA_CC_GENERIC},
    {Suggestion::Icon::kCardJCB, IDR_AUTOFILL_METADATA_CC_JCB},
    {Suggestion::Icon::kCardMasterCard, IDR_AUTOFILL_METADATA_CC_MASTERCARD},
    {Suggestion::Icon::kCardMir, IDR_AUTOFILL_METADATA_CC_MIR},
    {Suggestion::Icon::kCardTroy, IDR_AUTOFILL_METADATA_CC_TROY},
    {Suggestion::Icon::kCardUnionPay, IDR_AUTOFILL_METADATA_CC_UNIONPAY},
    {Suggestion::Icon::kCardVerve, IDR_AUTOFILL_METADATA_CC_VERVE},
    {Suggestion::Icon::kCardVisa, IDR_AUTOFILL_METADATA_CC_VISA},
    {Suggestion::Icon::kIban, IDR_AUTOFILL_IBAN},
    {Suggestion::Icon::kBnplGeneric, IDR_AUTOFILL_METADATA_BNPL_GENERIC},
    {Suggestion::Icon::kBnplAffirm, IDR_AUTOFILL_METADATA_AFFIRM},
    {Suggestion::Icon::kBnplAfterpay, IDR_AUTOFILL_METADATA_AFTERPAY},
    {Suggestion::Icon::kBnplKlarna, IDR_AUTOFILL_METADATA_KLARNA},
    {Suggestion::Icon::kBnplZip, IDR_AUTOFILL_METADATA_ZIP},
});

}  // namespace

int GetIconResourceID(Suggestion::Icon resource_name) {
  if ((resource_name == Suggestion::Icon::kCardAmericanExpress) &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableNewAmexNetworkArt)) {
    return IDR_AUTOFILL_METADATA_CC_AMEX_NEW;
  }

  if (ShouldUseNewFopDisplay()) {
    auto it = kDataResources.find(resource_name);
    return it == kDataResources.end() ? kResourceNotFoundId : it->second;
  }
  auto it = kOldDataResources.find(resource_name);
  return it == kOldDataResources.end() ? kResourceNotFoundId : it->second;
}

}  // namespace autofill
