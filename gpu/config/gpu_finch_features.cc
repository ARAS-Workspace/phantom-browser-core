// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_finch_features.h"

#include <string_view>

#include "base/byte_size.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/synchronization/atomic_flag.h"
#include "build/build_config.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_features.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gl_utils.h"

namespace features {
namespace {

SkiaGraphiteFeatureParams g_skia_graphite_feature_params;

base::AtomicFlag& GetGraphiteParamsInitFlag() {
  static base::NoDestructor<base::AtomicFlag> flag;
  return *flag;
}

void InitSkiaGraphiteFeatureParams(const base::Feature* feature) {
  if (GetGraphiteParamsInitFlag().IsSet()) {
    return;
  }

  if (!feature) {
    g_skia_graphite_feature_params = SkiaGraphiteFeatureParams();
    GetGraphiteParamsInitFlag().Set();
    return;
  }

  g_skia_graphite_feature_params.dawn_skip_validation =
      base::FeatureParam<bool>(
          feature, "dawn_skip_validation",
          g_skia_graphite_feature_params.dawn_skip_validation)
          .Get();
  g_skia_graphite_feature_params.dawn_backend_validation =
      base::FeatureParam<bool>(
          feature, "dawn_backend_validation",
          g_skia_graphite_feature_params.dawn_backend_validation)
          .Get();
  g_skia_graphite_feature_params.dawn_backend_debug_labels =
      base::FeatureParam<bool>(
          feature, "dawn_backend_debug_labels",
          g_skia_graphite_feature_params.dawn_backend_debug_labels)
          .Get();
  g_skia_graphite_feature_params.dawn_enable_auto_map =
      base::FeatureParam<bool>(
          feature, "dawn_enable_auto_map",
          g_skia_graphite_feature_params.dawn_enable_auto_map)
          .Get();
  g_skia_graphite_feature_params.max_pending_recordings =
      base::FeatureParam<int>(
          feature, "max_pending_recordings",
          g_skia_graphite_feature_params.max_pending_recordings)
          .Get();
  g_skia_graphite_feature_params.enable_deferred_submit =
      base::FeatureParam<bool>(
          feature, "enable_deferred_submit",
          g_skia_graphite_feature_params.enable_deferred_submit)
          .Get();
  g_skia_graphite_feature_params.enable_msaa_on_newer_intel =
      base::FeatureParam<bool>(
          feature, "enable_msaa_on_newer_intel",
          g_skia_graphite_feature_params.enable_msaa_on_newer_intel)
          .Get();

  GetGraphiteParamsInitFlag().Set();
}

}  // namespace

// More aggressive behavior for the shader cache: increase size, and do not
// purge as much in case of memory pressure.
BASE_FEATURE(kAggressiveShaderCacheLimits, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, gives GpuChannel/Host its own dedicated Mojo pipe instead
// of associating with an unused IPC::Channel.
BASE_FEATURE(kRemoveGPULegacyIPC, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_LINUX)
// Feature flag to control whether SharedImageStub sequence uses high priority
// on Linux. Enabled by default.
BASE_FEATURE(kSharedImageStubHighPriority, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// Enable GPU Rasterization by default. This can still be overridden by
// --enable-gpu-rasterization or --disable-gpu-rasterization.
// DefaultEnableGpuRasterization has launched on Mac, Windows, ChromeOS,
// Android and Linux.
BASE_FEATURE(kDefaultEnableGpuRasterization,
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enables the use of MSAA in skia on Ice Lake and later intel architectures.

BASE_FEATURE(kEnableMSAAOnNewIntelGPUs, base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// If enabled, the TASK_CATEGORY_POLICY value of the GPU process will be
// adjusted to match the one from the browser process every time it changes.
BASE_FEATURE(kAdjustGpuProcessPriority, base::FEATURE_DISABLED_BY_DEFAULT);
#endif

// When enabled, Grshader disk cache will be cleared on startup if any cache
// entry prefix does not match with the current prefix. prefix is made up of
// various parameters like chrome version, driver version etc.
BASE_FEATURE(kClearGrShaderDiskCacheOnInvalidPrefix,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Chrome will use the shader disk cache. This feature provides a
// kill-switch for working around issues with the disk cache and assessing the
// performance value of the disk cache. The --disable-gpu-shader-disk-cache flag
// overrides this feature and forces the disk cache to be disabled.
BASE_FEATURE(kGpuShaderDiskCache, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsShaderDiskCacheEnabled(const base::CommandLine* command_line) {
  if (command_line->HasSwitch(switches::kDisableGpuShaderDiskCache)) {
    return false;
  }

  return base::FeatureList::IsEnabled(kGpuShaderDiskCache);
}

// Enable Vulkan graphics backend for compositing and rasterization. Defaults to
// native implementation if --use-vulkan flag is not used. Otherwise
// --use-vulkan will be followed.
// Note Android WebView uses kWebViewDrawFunctorUsesVulkan instead of this.
BASE_FEATURE(kVulkan, base::FEATURE_DISABLED_BY_DEFAULT);

// Force enable WebGPU interop when enabled. When disabled the webgpu interop
// mechanism will default to auto detection in 'GetWebGPUOnVulkanViaGLInterop'
// function.
BASE_FEATURE(kForceEnableWebGpuInterop, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableDrDc,
#if BUILDFLAG(IS_MAC)
             // DrDC will not be running if Graphite is disabled on Mac.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             // NOT SUPPORTED. DO NOT ENABLE!
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Enable WebGPU on gpu service side only. This is used with origin trial and
// enabled by default on supported platforms.
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(USE_WEBGPU_ON_VULKAN_VIA_GL_INTEROP)
#define WEBGPU_ENABLED base::FEATURE_ENABLED_BY_DEFAULT
#else
#define WEBGPU_ENABLED base::FEATURE_DISABLED_BY_DEFAULT
#endif
BASE_FEATURE(kWebGPUService, WEBGPU_ENABLED);
BASE_FEATURE(kWebGPUBlobCache, WEBGPU_ENABLED);
#undef WEBGPU_ENABLED

// Feature enforces WebGPU security in Android Advanced Protection Mode.
// Disable feature by default for Finch testing.
BASE_FEATURE(kAAPMBlocksWebGPU, base::FEATURE_ENABLED_BY_DEFAULT);

// List of Dawn toggles for WebGPU, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:DisabledToggles/toggle1%2Ctoggle2
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWebGPUDisabledToggles{
    &kWebGPUService, "DisabledToggles", ""};
const base::FeatureParam<std::string> kWebGPUEnabledToggles{
    &kWebGPUService, "EnabledToggles", ""};
// List of WebGPU feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeFeatures/timestamp-query%2Cshader-f16
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWebGPUUnsafeFeatures{
    &kWebGPUService, "UnsafeFeatures", ""};
// Whether to enable Dawn's spontaneous wire mode on the server side for faster
// async resolution and timed wait any on the client side.
const base::FeatureParam<bool> kWebGPUSpontaneousWireServer{
    &kWebGPUService, "DawnSpontaneousWireServer", true};
// List of WGSL feature names, delimited by ,
// The FeatureParam may be overridden via Finch config, or via the command line
// For example:
//   --enable-field-trial-config \
//   --force-fieldtrial-params=WebGPU.Enabled:UnsafeWGSLFeatures/feature_1%2Cfeature_2
// Note that the comma should be URL-encoded.
const base::FeatureParam<std::string> kWGSLUnsafeFeatures{
    &kWebGPUService, "UnsafeWGSLFeatures", ""};

BASE_FEATURE(kWebGPUEnableRangeAnalysisForRobustness,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseSpirv14, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUDecomposeUniformBuffers, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseHLSL2021, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUUseSpirvReconvergenceMode,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable Skia Graphite with the platform's default Dawn backend.
// Note: This can be overridden by --enable-skia-graphite and
// --disable-skia-graphite which take precedence over the feature flag, and the
// Dawn backend can be overridden with the --skia-graphite-dawn-backend flag.
BASE_FEATURE(kSkiaGraphite,
#if BUILDFLAG(IS_APPLE)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

// Controls Skia Graphite specifically for Intel GPUs on Windows.
// On Windows, the status of Graphite on Intel GPUs won't be controlled
// by the standard SkiaGraphite feature, but by this feature flag
// instead. This feature only works if `kLateGraphiteFeatureCheck` is
// also enabled.
BASE_FEATURE(kSkiaGraphiteWinIntel,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows CompoundImageBacking to allocate backings during runtime if a
// compatible backing to serve clients requested usage is not already present.
BASE_FEATURE(kUseDynamicBackingAllocations, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this feature allows ClientSharedImage to store and use a
// scoped_refptr to SharedImageInterface, instead of the raw_ptr as used in
// SharedImageInterfaceHolder.
BASE_FEATURE(kUseStrongRefToSharedImageInterface,
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, this feature lets ClientSharedImage handle all SyncToken
// management (i.e. generation, waiting and storing) internally. All SyncTokens
// that clients obtain from ClientSharedImage will be empty in this situation.
BASE_FEATURE(kUseAutomaticSyncTokenManagement,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable atlasing of small paths on Skia Graphite. Only meaningful if
// SkiaGraphite is also enabled.
BASE_FEATURE(kSkiaGraphiteSmallPathAtlas, base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the Graphite feature check (including blocklist) is deferred to
// the GPU process rather than evaluated in the browser process.
BASE_FEATURE(kLateGraphiteFeatureCheck,
             base::FEATURE_DISABLED_BY_DEFAULT
);

// Enable Skia Graphite's Pipeline precompilation feature.
// Note: This is only meaningful when Skia Graphite is enabled but can then also
// be overridden by
// --enable-skia-graphite-precompilation and
// --disable-skia-graphite-precompilation.
BASE_FEATURE(kSkiaGraphitePrecompilation, base::FEATURE_DISABLED_BY_DEFAULT);

// Whether to use PersistentCache for Skia Graphite's pipeline cache.
BASE_FEATURE(kSkiaGraphiteUsePersistentCache,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool SkiaGraphiteUsesPersistentCache() {
  return base::FeatureList::IsEnabled(kSkiaGraphiteUsePersistentCache);
}

BASE_FEATURE(kConditionallySkipGpuChannelFlush,
             base::FEATURE_ENABLED_BY_DEFAULT
);

const SkiaGraphiteFeatureParams& GetSkiaGraphiteFeatureParams() {
  DCHECK(GetGraphiteParamsInitFlag().IsSet());
  return g_skia_graphite_feature_params;
}

void InitSkiaGraphiteDefaultParamsForTesting() {
  InitSkiaGraphiteFeatureParams(nullptr);
}

const base::FeatureParam<int> kSkiaGraphiteMinPathSizeForMsaa{
    &kSkiaGraphiteSmallPathAtlas, "min_path_size_for_msaa", 0};

// Whether to use the GpuPersistentCache for caching GPU process shader blobs.
// Usage for Graphite is controlled independently with
// kSkiaGraphiteDawnUsePersistentCache.
BASE_FEATURE(kGpuPersistentCache,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGpuPersistentCacheMetadata, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kGpuPersistentCacheMetadataPreloadCount{
    &kGpuPersistentCacheMetadata, "preload_count", 50};

// Use a 100-command limit before forcing context switch per command buffer
// instead of 20.
BASE_FEATURE(kIncreasedCmdBufferParseSlice, base::FEATURE_DISABLED_BY_DEFAULT);

// Prune transfer cache entries not accessed recently. This also turns off
// similar logic in cc::GpuImageDecodeCache which is the largest (often single)
// client of transfer cache.
BASE_FEATURE(kPruneOldTransferCacheEntries, base::FEATURE_ENABLED_BY_DEFAULT);

// On platforms with delegated compositing, try to release overlays later, when
// no new frames are swapped.
BASE_FEATURE(kDeferredOverlaysRelease,
             "DeferredOverlayRelease",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This feature allows enabling specific entries in
// software_rendering_list.json, via experimentation. The entries must have
// test_group property and test_group feature parameter should be set in the
// experiment for the entries that need to be enabled.
BASE_FEATURE(kGPUBlockListTestGroup, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGPUBlockListTestGroupId{&kGPUBlockListTestGroup,
                                                       "test_group", 0};

// This feature allows enabling specific entries in gpu_driver_bug_list.json,
// via experimentation. The entries must have test_group property and
// test_group feature parameter should be set in the experiment for the entries
// that need to be enabled.
BASE_FEATURE(kGPUDriverBugListTestGroup, base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kGPUDriverBugListTestGroupId{
    &kGPUDriverBugListTestGroup, "test_group", 0};

#if BUILDFLAG(IS_LINUX)
bool IsForceEnableWebGpuInterop() {
  return base::FeatureList::IsEnabled(kForceEnableWebGpuInterop);
}
#endif

bool IsUsingVulkan() {
#if BUILDFLAG(ENABLE_VULKAN)
  return base::FeatureList::IsEnabled(kVulkan);
#else
  return false;
#endif
}

bool IsUsingThreadSafeMediaForWebView() {
  return false;
}

// Note that DrDc is also disabled on some of the gpus (crbug.com/1354201).
// Thread safe media will still be used on those gpus which should be fine for
// now as the lock shouldn't have much overhead and is limited to only few gpus.
// This should be fixed/updated later to account for disabled gpus.
bool NeedThreadSafeAndroidMedia() {
  // If GpuFeatureInfo is available, replace ShouldEnableDrDc() with
  // IsDrDcEnabled(gpu_feature_info) which is set after checking drdc
  // workarounds;
  return ShouldEnableDrDc() || IsUsingThreadSafeMediaForWebView();
}

namespace {
bool IsSkiaGraphiteSupportedByDevice(const base::CommandLine* command_line) {
#if BUILDFLAG(IS_APPLE)
  // Graphite only works well with ANGLE Metal on Mac or iOS.
  // TODO(https://crbug.com/40063538): Remove this after ANGLE Metal launches
  // fully.
  const bool is_angle_metal_selected =
      base::FeatureList::IsEnabled(features::kDefaultANGLEMetal) ||
      command_line->GetSwitchValueASCII(switches::kUseANGLE) ==
          gl::kANGLEImplementationMetalName;
  return UsePassthroughCommandDecoder() && is_angle_metal_selected;
#else
  // Disallow Graphite from being enabled via the base::Feature on
  // not-yet-supported platforms to avoid users experiencing undefined behavior,
  // including behavior that might prevent them from being able to return to
  // chrome://flags to disable the feature.
  if (base::FeatureList::IsEnabled(features::kSkiaGraphite)) {
    LOG(ERROR) << "Enabling Graphite on a not-yet-supported platform is "
                  "disallowed for safety";
  }
  return false;
#endif
}
}  // namespace

bool IsSkiaGraphiteEnabled(const base::CommandLine* command_line) {
  // Force disabling graphite if --disable-skia-graphite flag is specified.
  if (command_line->HasSwitch(switches::kDisableSkiaGraphite)) {
    return false;
  }

  // Force Graphite on if --enable-skia-graphite flag is specified.
  if (command_line->HasSwitch(switches::kEnableSkiaGraphite)) {
    // The flag enable-skia-graphite is for testing purpose, so set the params
    // to default values.
    InitSkiaGraphiteFeatureParams(nullptr);
    return true;
  }

  if (!IsSkiaGraphiteSupportedByDevice(command_line)) {
    // Return early before checking "SkiaGraphite" feature so that devices
    // which don't support graphite are not included in the finch study.
    return false;
  }

  if (base::FeatureList::IsEnabled(kSkiaGraphite)) {
    InitSkiaGraphiteFeatureParams(&kSkiaGraphite);
    return true;
  }
  return false;
}

bool IsSkiaGraphiteWinIntelEnabled() {
  if (base::FeatureList::IsEnabled(kSkiaGraphiteWinIntel)) {
    InitSkiaGraphiteFeatureParams(&kSkiaGraphiteWinIntel);
    return true;
  }
  return false;
}

bool IsDrDcEnabled(const gpu::GpuFeatureInfo& gpu_feature_info) {
  return gpu_feature_info.status_values
             [gpu::GPU_FEATURE_TYPE_DIRECT_RENDERING_DISPLAY_COMPOSITOR] ==
         gpu::kGpuFeatureStatusEnabled;
}

bool ShouldEnableDrDc() {
  return base::FeatureList::IsEnabled(kEnableDrDc);
}

bool IsSkiaGraphitePrecompilationEnabled(
    const base::CommandLine* command_line) {
  // Force disabling Graphite Precompilation if
  // --disable-skia-graphite-precompilation flag is specified.
  if (command_line->HasSwitch(switches::kDisableSkiaGraphitePrecompilation)) {
    return false;
  }

  // Force Graphite Precompilation on if --enable-skia-graphite-precompilation
  // flag is specified.
  if (command_line->HasSwitch(switches::kEnableSkiaGraphitePrecompilation)) {
    return true;
  }

  return base::FeatureList::IsEnabled(features::kSkiaGraphitePrecompilation);
}

bool EnablePruneOldTransferCacheEntries() {
  return base::FeatureList::IsEnabled(kPruneOldTransferCacheEntries);
}

bool IsLegacyIpcDisabled() {
  return base::FeatureList::IsEnabled(kRemoveGPULegacyIPC);
}

// When this flag is enabled, stops using gpu::SyncPointOrderData for sync point
// validation, uses gpu::TaskGraph instead.
// Graph-based validation doesn't require sync point releases are submitted to
// the scheduler prior to their corresponding waits. Therefore it allows to
// remove the synchronous flush done by VerifySyncTokens().
//
// TODO(b/324276400): Work in progress.
BASE_FEATURE(kSyncPointGraphValidation, base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSyncPointGraphValidationEnabled() {
  return base::FeatureList::IsEnabled(kSyncPointGraphValidation);
}

BASE_FEATURE(kANGLEPerContextBlobCache, base::FEATURE_DISABLED_BY_DEFAULT);

// Support thread safety for graphite::context by sharing the same
// graphite::context as well as its wrapper class GraphiteSharedContext between
// GpuMain and CompositorGpuThread. Note: When this feature is disabled,
// each thread creates its own graphite::context and the context wrapper.
BASE_FEATURE(kGraphiteContextIsThreadSafe,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsGraphiteContextThreadSafe() {
  return base::FeatureList::IsEnabled(features::kGraphiteContextIsThreadSafe);
}

BASE_FEATURE(kWebGPUCompatibilityMode, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWebGPUAndroidOpenGLES, base::FEATURE_ENABLED_BY_DEFAULT);

// Enables runtime configuration of the GPU watchdog timeout via
// experimentation.
BASE_FEATURE(kConfigurableGPUWatchdogTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kConfigurableGPUWatchdogTimeoutSeconds{
    &kConfigurableGPUWatchdogTimeout, "watchdog_timeout_seconds", 30};

// Enables the optimization where GPU channels are sent to renderer processes
// early when the renderer process is being initialized, instead of waiting
// for the renderer to request the GPU channel to the browser process.
BASE_FEATURE(kSendGPUChannelEarly, base::FEATURE_DISABLED_BY_DEFAULT);
// If true, only enable the early GPU channel optimization for topchrome WebUI
// renderers.
const base::FeatureParam<bool> kSendGPUChannelEarlyTopChromeOnly{
    &kSendGPUChannelEarly, "for_topchrome_webui_only", false};

}  // namespace features
