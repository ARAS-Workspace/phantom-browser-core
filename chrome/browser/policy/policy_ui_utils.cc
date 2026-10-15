// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_ui_utils.h"

#include <optional>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/version/version_ui.h"
#include "chrome/common/channel_info.h"
#include "components/policy/core/browser/webui/json_generation.h"
#include "components/version_info/version_info.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace policy {

JsonGenerationParams GetChromeMetadataParams(
    const std::string& application_name) {
  std::optional<std::string> cohort_name;
  std::optional<std::string> os_name;
  std::optional<std::string> platform_name;
#if BUILDFLAG(IS_MAC)
  os_name = base::mac::GetOSDisplayName();
#else
  os_name = version_info::GetOSType();
#endif
  policy::JsonGenerationParams params;
  params.with_application_name(application_name)
      .with_channel_name(
          chrome::GetChannelName(chrome::WithExtendedStable(true)))
      .with_processor_variation(
          l10n_util::GetStringUTF8(VersionUI::VersionProcessorVariation()));

  if (cohort_name) {
    params.with_cohort_name(cohort_name.value());
  }

  if (os_name) {
    params.with_os_name(os_name.value());
  }

  if (platform_name) {
    params.with_platform_name(platform_name.value());
  }
  return params;
}
}  // namespace policy
