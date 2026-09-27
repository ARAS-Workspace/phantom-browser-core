// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_CONFIG_GPU_UTIL_H_
#define GPU_CONFIG_GPU_UTIL_H_

#include "build/build_config.h"
#include "gpu/config/gpu_config_export.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_mode.h"
#include "gpu/config/gpu_preferences.h"
#include "ui/gl/gl_display.h"

namespace base {
class CommandLine;
}

namespace gpu {

struct DevicePerfInfo;
struct GPUInfo;
struct GpuPreferences;
enum class IntelGpuSeriesType;
enum class IntelGpuGeneration;

// Set GPU feature status if GPU is blocked.
GPU_CONFIG_EXPORT GpuFeatureInfo ComputeGpuFeatureInfoWithNoGpu();

// Set GPU feature status for software GL implementations.
GPU_CONFIG_EXPORT GpuFeatureInfo ComputeGpuFeatureInfoForSoftwareGL();

// This function should only be called from the GPU process, or the Browser
// process while using in-process GPU. This function is safe to call at any
// point, and is not dependent on sandbox initialization.
// This function also appends a few commandline switches caused by driver bugs.
GPU_CONFIG_EXPORT GpuFeatureInfo
ComputeGpuFeatureInfo(const GPUInfo& gpu_info,
                      const GpuPreferences& gpu_preferences,
                      base::CommandLine* command_line,
                      bool* needs_more_info);

// Returns true if |gr_context_type| is supported according to
// |gpu_feature_info|, i.e. the feature corresponding to it is enabled:
//   - kGraphiteDawn -> GPU_FEATURE_TYPE_SKIA_GRAPHITE
//   - kVulkan       -> GPU_FEATURE_TYPE_VULKAN
//   - kGL           -> GPU_FEATURE_TYPE_ACCELERATED_GL
// kNone is always supported.
GPU_CONFIG_EXPORT bool IsGrContextTypeSupported(
    GrContextType gr_context_type,
    const GpuFeatureInfo& gpu_feature_info);

// Checks whether the current GrContextType in |gpu_preferences| is supported
// according to |gpu_feature_info| (see IsGrContextTypeSupported). If not, the
// function will switch to the next type from
// |gpu_preferences.fallback_gr_context_types|, recompute the feature info for
// that type, and repeat until a supported type is found or all candidates are
// exhausted. Returns true if a supported type was found.
GPU_CONFIG_EXPORT bool TryFallbackGrContextTypesIfNeeded(
    GpuFeatureInfo& gpu_feature_info,
    GpuPreferences& gpu_preferences,
    const GPUInfo& gpu_info,
    base::CommandLine* command_line);

// Maps GpuMode to the hardware context type it supports. Non-hardware modes
// (e.g. SOFTWARE_GL, DISPLAY_COMPOSITOR, UNKNOWN) return GrContextType::kNone.
GPU_CONFIG_EXPORT GrContextType GpuModeToGrContextType(GpuMode mode);

GPU_CONFIG_EXPORT void SetKeysForCrashLogging(const GPUInfo& gpu_info);

// Returns whether SwiftShader should be enabled. If true, the proper command
// line switch to enable SwiftShader will be appended to 'command_line'.
GPU_CONFIG_EXPORT bool EnableSwiftShaderIfNeeded(
    base::CommandLine* command_line,
    const GpuFeatureInfo& gpu_feature_info,
    bool disable_software_rasterizer,
    bool blocklist_needs_more_info);

GPU_CONFIG_EXPORT IntelGpuSeriesType GetIntelGpuSeriesType(uint32_t vendor_id,
                                                           uint32_t device_id);

GPU_CONFIG_EXPORT IntelGpuGeneration GetIntelGpuGeneration(uint32_t vendor_id,
                                                           uint32_t device_id);

// If multiple Intel GPUs are detected, this returns the latest generation.
GPU_CONFIG_EXPORT IntelGpuGeneration
GetIntelGpuGeneration(const GPUInfo& gpu_info);

// If this function is called in browser process (|in_browser_process| is set
// to true), don't collect total disk space (which may block) and D3D related
// info.
GPU_CONFIG_EXPORT void CollectDevicePerfInfo(DevicePerfInfo* device_perf_info,
                                             bool in_browser_process);
GPU_CONFIG_EXPORT void RecordDevicePerfInfoHistograms();

// In a multi-gpu device, record the discrete gpu device id.
// Currently only record for AMD/Nvidia GPUs.
GPU_CONFIG_EXPORT void RecordDiscreteGpuHistograms(const GPUInfo& gpu_info);

// Record histograms for NPU device id.
// Currently only record for Intel NPUs.
GPU_CONFIG_EXPORT void RecordNpuHistograms(const GPUInfo& gpu_info);

#if BUILDFLAG(IS_MAC)
GPU_CONFIG_EXPORT void TrySetNonSoftwareDevicePreferenceForTesting(
    gl::GpuPreference gpu_preference);
#endif

}  // namespace gpu

#endif  // GPU_CONFIG_GPU_UTIL_H_
