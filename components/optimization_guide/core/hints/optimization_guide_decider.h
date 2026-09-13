// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECIDER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECIDER_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"

class GURL;

namespace optimization_guide {

class OptimizationGuideDecider {
 public:
  // Registers the optimization types that intend to be queried during the
  // session. It is expected for this to be called after the browser has been
  // initialized.
  //
  // Registering an OptimizationType will cause data for the type to be fetched
  // and cached on each page load.
  virtual void RegisterOptimizationTypes(
      const std::vector<proto::OptimizationType>& optimization_types) = 0;

  // Invokes |callback| with the decision for the URL contained in |url| and
  // |optimization_type|, when sufficient information has been collected to
  // make the decision.
  virtual void CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationGuideDecisionCallback callback) = 0;

  // Returns whether |optimization_type| can be applied for |url|. This should
  // only be called for main frame navigations or future main frame navigations.
  virtual OptimizationGuideDecision CanApplyOptimization(
      const GURL& url,
      proto::OptimizationType optimization_type,
      OptimizationMetadata* optimization_metadata) = 0;

 protected:
  OptimizationGuideDecider() = default;
  virtual ~OptimizationGuideDecider() = default;

};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_HINTS_OPTIMIZATION_GUIDE_DECIDER_H_
