// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network/network_service_util_internal.h"

#include "base/byte_size.h"
#include "base/check.h"
#include "base/command_line.h"
#include "build/build_config.h"
#include "content/browser/network/network_service_util_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

std::optional<bool> g_force_network_service_process_in_or_out;

}  // namespace

void ForceInProcessNetworkServiceImpl() {
  CHECK(!g_force_network_service_process_in_or_out ||
        *g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = true;
}
void ForceOutOfProcessNetworkServiceImpl() {
  CHECK(!g_force_network_service_process_in_or_out ||
        !*g_force_network_service_process_in_or_out);
  g_force_network_service_process_in_or_out = false;
}

bool IsInProcessNetworkServiceImpl() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    return true;
  }

  if (g_force_network_service_process_in_or_out) {
    return *g_force_network_service_process_in_or_out;
  }

  return base::FeatureList::IsEnabled(features::kNetworkServiceInProcess);
}

}  // namespace content
