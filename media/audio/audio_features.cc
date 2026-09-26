// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"

namespace features {

// This feature flag controls whether the WebAudio destination resampler is
// bypassed. When enabled, if the WebAudio context's sample rate differs from
// the hardware's sample rate, the resampling step that normally occurs within
// the WebAudio destination node is skipped. This allows the AudioService to
// handle any necessary resampling, potentially reducing latency and overhead.
BASE_FEATURE(kWebAudioRemoveAudioDestinationResampler,
             base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_MAC)
// Enabling this feature will allow AudioManagerMac to generate AVFoundation
// AudioOutputStreams instead of AUHALStreams in cases of multichannel audio.
// MacOS will then "Spatialize" the audio for users on compatible Airpods. The
// end result will give users the option to change modes on their Airpods (Off,
// Fixed, Head Tracking).
BASE_FEATURE(kMacAVFoundationPlayback, base::FEATURE_DISABLED_BY_DEFAULT);

// If this feature is enabled, and CATap is capturing the default output device,
// the CATap implementation will handle default output device changes by
// restarting the system audio capture. The changes we listen for are if
// default output device is changed to another device, or if the sample rate of
// the default output device is changed. If the feature is disabled, CATap will
// keep capturing the same device when default output device is changed, and
// will report an error if the sample rate is changed.
BASE_FEATURE(kMacCatapRestartOnDeviceChange, base::FEATURE_ENABLED_BY_DEFAULT);

// If this feature is enabled, the audio process is restarted if the
// AudioDeviceCreateIOProcID call times out. This is used to recover from
// permission dialogs that are not responded to.
BASE_FEATURE(kMacCatapRestartAudioProcessOnTimeout,
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif

}  // namespace features
