// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_
#define COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_

#include <string>

#include "build/build_config.h"

namespace continuous_search {
struct Config {
  // The ID of the related searches container.
  std::string related_searches_id = "bres";

  // The classname of the related searches anchor element.
  std::string related_searches_anchor_classname = "k8XOCe";

  // The classname of the related searches title element.
  std::string related_searches_title_classname = "s75CSd";

  Config();
  Config(const Config& other);
  ~Config();
};

// Gets the current configuration.
const Config& GetConfig();

// Overrides the config returned by |GetConfig()|.
void SetConfigForTesting(const Config& config);

}  // namespace continuous_search

#endif  // COMPONENTS_CONTINUOUS_SEARCH_RENDERER_CONFIG_H_