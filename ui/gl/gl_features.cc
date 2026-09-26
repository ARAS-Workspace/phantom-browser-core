// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_features.h"

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace features {
namespace {

BASE_FEATURE(kForceANGLEFeatures, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<std::string> kForcedANGLEEnabledFeaturesFP{
    &kForceANGLEFeatures, "EnabledFeatures", ""};
const base::FeatureParam<std::string> kForcedANGLEDisabledFeaturesFP{
    &kForceANGLEFeatures, "DisabledFeatures", ""};

void SplitAndAppendANGLEFeatureList(const std::string& list,
                                    std::vector<std::string>& out_features) {
  for (std::string& feature_name : base::SplitString(
           list, ", ;", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    out_features.push_back(std::move(feature_name));
  }
}

}  // namespace

#if BUILDFLAG(IS_APPLE)
BASE_FEATURE(kGpuVsync, base::FEATURE_DISABLED_BY_DEFAULT);
#else
BASE_FEATURE(kGpuVsync, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
// Use the passthrough command decoder by default.  This can be overridden with
// the --use-cmd-decoder=passthrough or --use-cmd-decoder=validating flags.
// Feature lives in ui/gl because it affects the GL binding initialization on
// platforms that would otherwise not default to using EGL bindings.
BASE_FEATURE(kDefaultPassthroughCommandDecoder,
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)

bool UseGpuVsync() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kDisableGpuVsync) &&
         base::FeatureList::IsEnabled(features::kGpuVsync);
}

bool IsAndroidFrameDeadlineEnabled() {
  return false;
}

bool UsePassthroughCommandDecoder() {
#if !BUILDFLAG(ENABLE_VALIDATING_COMMAND_DECODER)
  return true;
#else

  if (!base::FeatureList::IsEnabled(kDefaultPassthroughCommandDecoder))
    return false;

  return true;
#endif  // defined(PASSTHROUGH_COMMAND_DECODER_LAUNCHED)
}

#if DCHECK_IS_ON()
bool IsANGLEValidationEnabled() {
  return true;
}
#else
// Enables the use of ANGLE validation for EGL and GL (non-WebGL) contexts.
BASE_FEATURE(kDefaultEnableANGLEValidation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsANGLEValidationEnabled() {
  return base::FeatureList::IsEnabled(kDefaultEnableANGLEValidation) &&
         UsePassthroughCommandDecoder();
}
#endif

// Killswitch feature for allowing ANGLE to pass untranslated shaders to the
// driver.
BASE_FEATURE(kAllowANGLEPassthroughShaders, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsANGLEPassthroughShadersAllowed() {
  return base::FeatureList::IsEnabled(kAllowANGLEPassthroughShaders);
}

void GetANGLEFeaturesFromCommandLineAndFinch(
    const base::CommandLine* command_line,
    std::vector<std::string>& enabled_angle_features,
    std::vector<std::string>& disabled_angle_features) {
  SplitAndAppendANGLEFeatureList(
      command_line->GetSwitchValueASCII(switches::kEnableANGLEFeatures),
      enabled_angle_features);
  SplitAndAppendANGLEFeatureList(
      command_line->GetSwitchValueASCII(switches::kDisableANGLEFeatures),
      disabled_angle_features);

  if (base::FeatureList::IsEnabled(kForceANGLEFeatures)) {
    SplitAndAppendANGLEFeatureList(kForcedANGLEEnabledFeaturesFP.Get(),
                                   enabled_angle_features);
    SplitAndAppendANGLEFeatureList(kForcedANGLEDisabledFeaturesFP.Get(),
                                   disabled_angle_features);
  }
}

bool ShouldFallbackToSWIfGLES3NotSupported() {
  return true;
}

#if BUILDFLAG(ENABLE_SWIFTSHADER)
bool IsSwiftShaderAllowedByCommandLine(const base::CommandLine* command_line) {
  // If the switch to opt-into unsafe SwiftShader is present, always allow
  // SwiftShader.
  if (command_line->HasSwitch(switches::kEnableUnsafeSwiftShader)) {
    return true;
  }

  std::string angle_name =
      command_line->GetSwitchValueASCII(switches::kUseANGLE);
  if (angle_name == gl::kANGLEImplementationSwiftShaderName ||
      angle_name == gl::kANGLEImplementationSwiftShaderForWebGLName) {
    // If SwiftShader is specifically requested with the --use-angle command
    // line flag, allow it.
    return true;
  }

  return false;
}

bool IsSwiftShaderUsedForWebGLByCommandLine(
    const base::CommandLine* command_line) {
  std::string use_gl = command_line->GetSwitchValueASCII(switches::kUseGL);
  if (!use_gl.empty() && use_gl != gl::kGLImplementationANGLEName) {
    return false;
  }
  return command_line->GetSwitchValueASCII(switches::kUseANGLE) ==
         gl::kANGLEImplementationSwiftShaderForWebGLName;
}

// Allow fallback to SwfitShader without command line flags during the
// deprecation period.
BASE_FEATURE(kAllowSwiftShaderFallback, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSwiftShaderAllowedByFeature() {
  return base::FeatureList::IsEnabled(kAllowSwiftShaderFallback);
}
#else
bool IsSwiftShaderAllowedByCommandLine(const base::CommandLine*) {
  return false;
}

bool IsSwiftShaderAllowedByFeature() {
  return false;
}

bool IsSwiftShaderUsedForWebGLByCommandLine(const base::CommandLine*) {
  return false;
}
#endif

bool IsSwiftShaderAllowed(const base::CommandLine* command_line) {
  return IsSwiftShaderAllowedByCommandLine(command_line) ||
         IsSwiftShaderAllowedByFeature();
}

bool IsWARPAllowed(const base::CommandLine*) {
  return false;
}

bool IsAnySoftwareGLAllowed(const base::CommandLine* command_line) {
  return IsWARPAllowed(command_line) || IsSwiftShaderAllowed(command_line);
}

BASE_FEATURE(kAllowSoftwareGLFallbackDueToCrashes,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSoftwareGLFallbackDueToCrashesAllowed(
    const base::CommandLine* command_line) {
  if (!IsAnySoftwareGLAllowed(command_line)) {
    return false;
  }

  return base::FeatureList::IsEnabled(kAllowSoftwareGLFallbackDueToCrashes);
}

}  // namespace features
