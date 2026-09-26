// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/frame_deadline_decider.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {
namespace {

constexpr base::TimeDelta k120HzVsyncInterval = base::Milliseconds(8);
constexpr int k120HzMaxPendingSwaps = 4;
constexpr int k120HzAllowedBuffers = k120HzMaxPendingSwaps + 1;

class FrameDeadlineDeciderTest : public testing::Test {
 public:
  FrameDeadlineDeciderTest() = default;
  ~FrameDeadlineDeciderTest() override = default;
};

PossibleDeadlines CreatePossibleDeadlines(
    size_t os_preferred_index,
    std::vector<PossibleDeadline> deadlines) {
  PossibleDeadlines possible_deadlines(os_preferred_index);
  possible_deadlines.deadlines = std::move(deadlines);
  return possible_deadlines;
}

TEST_F(FrameDeadlineDeciderTest, FeatureDisabledFallback) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(true);

  auto deadlines = CreatePossibleDeadlines(
      1, {PossibleDeadline(1, base::Milliseconds(4), base::Milliseconds(12)),
          PossibleDeadline(2, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(3, base::Milliseconds(12), base::Milliseconds(20))});

  EXPECT_EQ(1u, decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                                       k120HzAllowedBuffers, base::TimeTicks(),
                                       std::nullopt,
                                       /*is_handling_interaction=*/false));
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, SelectedSustainableDeadline_True) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/true);

  // k120HzAllowedBuffers = 5, k120HzVsyncInterval = 8 ms.
  // max_sustainable_delta = (5 * 8 ms) + 1 ms = 41 ms.
  // 41 ms <= 41 ms -> sustainable (true).
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(41))});

  decider.SelectDeadline(deadlines, k120HzVsyncInterval, k120HzAllowedBuffers,
                         base::TimeTicks(), std::nullopt,
                         /*is_handling_interaction=*/false);

  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectedSustainableDeadline", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, SelectedSustainableDeadline_False) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/true);

  // k120HzAllowedBuffers = 5, k120HzVsyncInterval = 8 ms.
  // max_sustainable_delta = (5 * 8 ms) + 1 ms = 41 ms.
  // 42 ms > 41 ms -> unsustainable (false).
  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(42))});

  decider.SelectDeadline(deadlines, k120HzVsyncInterval, k120HzAllowedBuffers,
                         base::TimeTicks(), std::nullopt,
                         /*is_handling_interaction=*/false);

  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectedSustainableDeadline", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kPlatformPreferred, 1);
}

TEST_F(FrameDeadlineDeciderTest, OngoingInteractionSequenceRetention) {
  base::HistogramTester histogram_tester;
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(features::FrameDeadlineDeciderSequenceStrategy::
                                    kPresentationDeltaLocking);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);
  histogram_tester.ExpectBucketCount(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kChromePreferredNewSequence, 1);

  // Frame 2 (t=150ms): Active interaction continues (> 50ms threshold).
  // Because both previous and current frames are interactive, sequence is
  // retained (locked to 40ms / index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(150), std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);
  histogram_tester.ExpectBucketCount(
      "Viz.FrameDeadlineDecider.SelectionReason",
      FrameDeadlineDecider::SelectionReason::kOngoingSequence, 1);
}

TEST_F(FrameDeadlineDeciderTest, InteractionEndSequenceRetention) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(features::FrameDeadlineDeciderSequenceStrategy::
                                    kPresentationDeltaLocking);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);

  // Frame 2 (t=20ms): Interaction ends (is_handling_interaction = false).
  // Arrives within 50ms grace period (20ms <= 50ms), so sequence is retained
  // for this frame (index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(20), std::nullopt,
                             /*is_handling_interaction=*/false),
      2u);
}

TEST_F(FrameDeadlineDeciderTest, InteractionEndGracePeriodExpiration) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interactive sequence. Locked to 40ms delta (index
  // 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      2u);

  // Frame 2 (t=60ms): Interaction ends (is_handling_interaction = false) after
  // 60ms gap (> 50ms threshold). Because interaction ended,
  // max_non_interactive_idle_duration_ (50ms) applies. 60ms > 50ms, so sequence
  // resets (recalculates for max_allowed_buffers = 3 -> 24ms / index 1).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(60), std::nullopt,
                             /*is_handling_interaction=*/false),
      1u);
}

TEST_F(FrameDeadlineDeciderTest, NonInteractiveToInteractiveTransitionTimeout) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start non-interactive sequence. Locked to 40ms delta
  // (index 2).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/false),
      2u);

  // Frame 2 (t=80ms): Interactive gesture starts (is_handling_interaction =
  // true) after 80ms gap. Because previous frame was non-interactive, the
  // transition frame uses max_non_interactive_idle_duration_ (50ms). 80ms >
  // 50ms, so preceding sequence resets and target deadline is recalculated for
  // max_allowed_buffers = 3 (24ms / index 1).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval, 3,
                             base_time + base::Milliseconds(80), std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);
}

TEST_F(FrameDeadlineDeciderTest,
       NotifyMinSupportedVsyncIntervalCapsPresentationDelta) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);

  auto deadlines_60hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(32)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(48)),
          PossibleDeadline(4, base::Milliseconds(48), base::Milliseconds(64))});

  // 1. Without NotifyMinSupportedVsyncInterval, target_present_delta (48ms)
  // selects Index 2 (48ms).
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            2u);

  decider.OnDisplayInvisible();

  // 2. After NotifyMinSupportedVsyncInterval(8ms) [120 Hz peak -> presentation
  // cap = 24ms], target_present_delta is capped to <= 24ms, selecting Index 0
  // (16ms) instead of Index 2.
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(8));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            0u);
}

TEST_F(FrameDeadlineDeciderTest,
       NotifyMinSupportedVsyncIntervalUpdatedDuringSession) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);

  auto deadlines_60hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(32)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(48)),
          PossibleDeadline(4, base::Milliseconds(48), base::Milliseconds(64))});

  // 1. Notify 16ms min supported vsync interval (60 Hz only display ->
  // presentation cap = 48ms). Target present delta (48ms) <= presentation cap
  // (48ms), so Index 2 (48ms) is selected.
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(16));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            2u);

  decider.OnDisplayInvisible();

  // 2. Display capabilities change (e.g., external 120 Hz display connected,
  // 8ms min vsync interval). Presentation cap is now 24ms. Target present delta
  // is capped to <= 24ms, selecting Index 0 (16ms).
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(8));
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, base::Milliseconds(16),
                                   /*max_allowed_buffers=*/3,
                                   base::TimeTicks::Now(), std::nullopt,
                                   /*is_handling_interaction=*/true),
            0u);
}

TEST_F(FrameDeadlineDeciderTest, OsPreferredDeltaLocking_StableSequence) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(4, base::Milliseconds(32), base::Milliseconds(40)),
          PossibleDeadline(5, base::Milliseconds(40), base::Milliseconds(48))});

  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  // Frame 0: Initial frame of sequence selects index 3 (40ms).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      3u);

  // Frames 1..9: Stable consecutive frames in sequence.
  for (int i = 1; i <= 9; ++i) {
    base::TimeTicks frame_time = base_time + (i * k120HzVsyncInterval);
    EXPECT_EQ(
        decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                               k120HzAllowedBuffers, frame_time, std::nullopt,
                               /*is_handling_interaction=*/true),
        3u);
  }
}

TEST_F(FrameDeadlineDeciderTest, OsPreferredDeltaLocking_VsyncJitterShift) {
  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines_f1 = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(4, base::Milliseconds(32), base::Milliseconds(40)),
          PossibleDeadline(5, base::Milliseconds(40), base::Milliseconds(48))});

  // VSync jitter (+3ms latch phase shift) on Frame 2.
  auto deadlines_f2_shifted = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(11), base::Milliseconds(19)),
          PossibleDeadline(2, base::Milliseconds(19), base::Milliseconds(27)),
          PossibleDeadline(3, base::Milliseconds(27), base::Milliseconds(35)),
          PossibleDeadline(4, base::Milliseconds(35), base::Milliseconds(43)),
          PossibleDeadline(5, base::Milliseconds(43), base::Milliseconds(51))});

  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  EXPECT_EQ(
      decider.SelectDeadline(deadlines_f1, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      3u);  // 40ms (offset = 40 - 16 = 24ms)

  // OS preferred is 19ms. Target = 19 + 24 = 43ms.
  // Max sustainable = 5 * 8ms + 1ms = 41ms.
  // Candidate 3 (43ms) > 41ms -> filtered out.
  // Sustainable candidates: [19ms, 27ms, 35ms]. Best: index 2 (35ms).
  EXPECT_EQ(decider.SelectDeadline(
                deadlines_f2_shifted, k120HzVsyncInterval, k120HzAllowedBuffers,
                base_time + k120HzVsyncInterval, std::nullopt,
                /*is_handling_interaction=*/true),
            2u);  // 35ms (SUSTAINABLE)
}

TEST_F(FrameDeadlineDeciderTest, OsPreferredDeltaLocking_RefreshRateSwitch) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  // Frame 1: 120Hz (8ms vsync), 5 buffers.
  auto deadlines_120hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(4, base::Milliseconds(32), base::Milliseconds(40))});

  EXPECT_EQ(
      decider.SelectDeadline(deadlines_120hz, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      3u);  // 40ms (offset = 40 - 16 = 24ms)

  // Frame 2: Switch to 60Hz (16.67ms vsync), 3 buffers.
  // Max sustainable = 3 * 16.67ms + 1ms = 51ms.
  constexpr base::TimeDelta k60HzVsyncInterval = base::Microseconds(16667);
  auto deadlines_60hz = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Microseconds(8333),
                           base::Microseconds(16667)),  // OS pref
          PossibleDeadline(2, base::Microseconds(16667),
                           base::Microseconds(33333)),  // 33.33ms
          PossibleDeadline(3, base::Microseconds(33333),
                           base::Microseconds(50000)),  // 50ms
          PossibleDeadline(
              4, base::Microseconds(50000),
              base::Microseconds(66667))});  // 66.67ms (unsustainable)

  // Target = 16.67ms + 24ms = 40.67ms.
  // Sustainable candidates: [16.67ms, 33.33ms, 50.00ms].
  // |33.33 - 40.67| = 7.34ms, |50.00 - 40.67| = 9.33ms.
  // Closest sustainable is index 1 (33.33ms).
  EXPECT_EQ(decider.SelectDeadline(deadlines_60hz, k60HzVsyncInterval,
                                   /*max_allowed_buffers=*/3,
                                   base_time + k60HzVsyncInterval, std::nullopt,
                                   /*is_handling_interaction=*/true),
            1u);
}

TEST_F(FrameDeadlineDeciderTest,
       OsPreferredDeltaLocking_BufferCountReductionClamping) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32)),
          PossibleDeadline(4, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1: 5 buffers (max sustainable = 41ms) -> selects index 3 (40ms).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      3u);

  // Frame 2: Buffers reduced from 5 to 3 (max sustainable = 3 * 8 + 1 = 25ms).
  // Target = 16 + 24 = 40ms.
  // Candidates > 25ms (index 2: 32ms, index 3: 40ms) are filtered out.
  // Sustainable candidates: index 0 (16ms), index 1 (24ms).
  // Closest to 40ms is index 1 (24ms).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines, k120HzVsyncInterval,
                             /*max_allowed_buffers=*/3,
                             base_time + k120HzVsyncInterval, std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);
}

TEST_F(FrameDeadlineDeciderTest, OsPreferredDeltaLocking_SequenceTimeoutReset) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);

  auto deadlines_5buf = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(32), base::Milliseconds(40))});

  auto deadlines_3buf = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(32), base::Milliseconds(40))});

  // Frame 1 (t=0ms): Start interaction sequence with 5 buffers -> selects 40ms
  // (index 1).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines_5buf, k120HzVsyncInterval,
                             k120HzAllowedBuffers, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);

  // Frame 2 (t=3500ms): Gap of 3.5s > 3.0s interactive timeout resets sequence
  // state. Recalculates new sequence target for 3 buffers: (3 * 8ms) = 24ms ->
  // selects index 1 (24ms).
  EXPECT_EQ(
      decider.SelectDeadline(deadlines_3buf, k120HzVsyncInterval,
                             /*max_allowed_buffers=*/3,
                             base_time + base::Milliseconds(3500), std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);
}

TEST_F(
    FrameDeadlineDeciderTest,
    OsPreferredDeltaLocking_NotifyMinSupportedVsyncIntervalCapsSustainableDelta) {
  FrameDeadlineDecider decider(/*use_platform_preferred_deadlines=*/false);
  decider.SetStrategyForTesting(
      features::FrameDeadlineDeciderSequenceStrategy::kOsPreferredDeltaLocking);

  // Notify display capability (supports 120Hz peak / 8ms min vsync interval)
  // before the interaction sequence begins.
  decider.NotifyMinSupportedVsyncInterval(base::Milliseconds(8));

  base::TimeTicks base_time = base::TimeTicks() + base::Milliseconds(1000);
  constexpr base::TimeDelta k60HzVsyncInterval = base::Milliseconds(16);

  auto deadlines_f1 = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(8), base::Milliseconds(16)),
          PossibleDeadline(2, base::Milliseconds(16), base::Milliseconds(24)),
          PossibleDeadline(3, base::Milliseconds(24), base::Milliseconds(32))});

  // Frame 1: Sequence starts at 60Hz (16ms vsync), 3 allowed buffers.
  // QueryDeadline caps initial target to min(3 * 16ms, 3 * 8ms) = 24ms.
  // Selects index 1 (24ms). Sequence offset = 24 - 16 = 8ms.
  EXPECT_EQ(
      decider.SelectDeadline(deadlines_f1, k60HzVsyncInterval,
                             /*max_allowed_buffers=*/3, base_time, std::nullopt,
                             /*is_handling_interaction=*/true),
      1u);

  // Frame 2 (subsequent frame in sequence at 60Hz):
  // VSync jitter (+3ms phase shift) shifts deadlines to [19ms, 27ms, 35ms].
  // Target delta = 19ms + 8ms = 27ms.
  // With min supported vsync interval = 8ms, max_sustainable_delta in
  // SelectDeadlineOsPreferredLocking is capped to (3 * 8ms) + 1ms = 25ms.
  // Candidate index 1 (27ms > 25ms) is filtered out.
  // The closest sustainable deadline is index 0 (19ms).
  auto deadlines_f2_shifted = CreatePossibleDeadlines(
      0, {PossibleDeadline(1, base::Milliseconds(11), base::Milliseconds(19)),
          PossibleDeadline(2, base::Milliseconds(19), base::Milliseconds(27)),
          PossibleDeadline(3, base::Milliseconds(27), base::Milliseconds(35))});

  EXPECT_EQ(decider.SelectDeadline(deadlines_f2_shifted, k60HzVsyncInterval,
                                   /*max_allowed_buffers=*/3,
                                   base_time + k60HzVsyncInterval, std::nullopt,
                                   /*is_handling_interaction=*/true),
            0u);
}

}  // namespace
}  // namespace viz
