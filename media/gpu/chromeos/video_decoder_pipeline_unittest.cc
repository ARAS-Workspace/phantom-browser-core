// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/video_decoder_pipeline.h"

#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/config/gpu_driver_bug_workarounds.h"
#include "media/base/cdm_context.h"
#include "media/base/media_util.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_media_log.h"
#include "media/base/status.h"
#include "media/base/video_decoder_config.h"
#include "media/gpu/chromeos/dmabuf_video_frame_pool.h"
#include "media/gpu/chromeos/frame_resource_converter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libdrm/src/include/drm/drm_fourcc.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::ByMove;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::TestWithParam;

namespace media {

using PixelLayoutCandidate = ImageProcessor::PixelLayoutCandidate;

MATCHER_P(MatchesStatusCode, status_code, "") {
  // media::Status doesn't provide an operator==(...), we add here a simple one.
  return arg.code() == status_code;
}

MATCHER_P(MatchesDecoderBuffer, buffer, "") {
  DCHECK(arg);
  return arg->MatchesForTesting(*buffer);
}

class FakeFrameResourceConverter : public FrameResourceConverter {
 public:
  static std::unique_ptr<FrameResourceConverter> Create() {
    return base::WrapUnique<FrameResourceConverter>(
        new FakeFrameResourceConverter());
  }

  FakeFrameResourceConverter(const FakeFrameResourceConverter&) = delete;
  FakeFrameResourceConverter& operator=(const FakeFrameResourceConverter&) =
      delete;

 private:
  FakeFrameResourceConverter() = default;
  ~FakeFrameResourceConverter() override = default;

  // FrameResourceConverter overrides.
  void ConvertFrameImpl(scoped_refptr<FrameResource> frame) override {
    // It is fine to output a nullptr VideoFrame. The output frames are not
    // checked.
    Output(nullptr);
  }
};

class MockVideoFramePool : public DmabufVideoFramePool {
 public:
  MockVideoFramePool() = default;
  ~MockVideoFramePool() override = default;

  // DmabufVideoFramePool implementation.
  MOCK_METHOD7(Initialize,
               CroStatus::Or<GpuBufferLayout>(const Fourcc&,
                                              const gfx::Size&,
                                              const gfx::Rect&,
                                              const gfx::Size&,
                                              size_t,
                                              bool,
                                              bool));
  MOCK_METHOD0(GetFrame, scoped_refptr<FrameResource>());
  MOCK_CONST_METHOD0(GetFrameStorageType, VideoFrame::StorageType());
  MOCK_METHOD0(IsExhausted, bool());
  MOCK_METHOD1(NotifyWhenFrameAvailable, void(base::OnceClosure));
  MOCK_METHOD0(ReleaseAllFrames, void());
  MOCK_METHOD0(GetGpuBufferLayout, std::optional<GpuBufferLayout>());

  bool IsFakeVideoFramePool() override { return true; }
};

class MockDecoder : public VideoDecoderMixin {
 public:
  MockDecoder()
      : VideoDecoderMixin(std::make_unique<MockMediaLog>(),
                          base::SequencedTaskRunner::GetCurrentDefault(),
                          base::WeakPtr<VideoDecoderMixin::Client>(nullptr)) {}
  ~MockDecoder() override = default;

  MOCK_METHOD6(Initialize,
               void(const VideoDecoderConfig&,
                    bool,
                    CdmContext*,
                    InitCB,
                    const PipelineOutputCB&,
                    const WaitingCB&));
  MOCK_METHOD2(Decode, void(scoped_refptr<DecoderBuffer>, DecodeCB));
  MOCK_METHOD1(Reset, void(base::OnceClosure));
  MOCK_METHOD0(ApplyResolutionChange, void());
  MOCK_METHOD0(NeedsTranscryption, bool());
  MOCK_METHOD1(AttachSecureBuffer, CroStatus(scoped_refptr<DecoderBuffer>&));
  MOCK_METHOD1(ReleaseSecureBuffer, void(uint64_t));
  MOCK_CONST_METHOD0(GetDecoderType, VideoDecoderType());
};

class MockImageProcessor : public ImageProcessor {
 public:
  explicit MockImageProcessor(
      scoped_refptr<base::SequencedTaskRunner> client_task_runner)
      : ImageProcessor(nullptr,
                       client_task_runner,
                       /*backend_task_runner=*/
                       base::ThreadPool::CreateSequencedTaskRunner({})) {}

  MOCK_CONST_METHOD0(input_config, const ImageProcessorBackend::PortConfig&());
  MOCK_CONST_METHOD0(output_config, const ImageProcessorBackend::PortConfig&());
};

struct DecoderPipelineTestParams {
  // GTest params need to be copyable; hence we need here a RepeatingCallback
  // version of VideoDecoderPipeline::CreateDecoderFunctionCB.
  using RepeatingCreateDecoderFunctionCB = base::RepeatingCallback<
      VideoDecoderPipeline::CreateDecoderFunctionCB::RunType>;
  RepeatingCreateDecoderFunctionCB create_decoder_function_cb;
  DecoderStatus::Codes status_code;
};

constexpr gfx::Size kMinSupportedResolution(64, 64);
constexpr gfx::Size kMaxSupportedResolution(2048, 1088);
constexpr gfx::Size kCodedSize(128, 128);

static_assert(kMinSupportedResolution.width() <= kCodedSize.width() &&
                  kMinSupportedResolution.height() <= kCodedSize.height() &&
                  kCodedSize.width() <= kMaxSupportedResolution.width() &&
                  kCodedSize.height() <= kMaxSupportedResolution.height(),
              "kCodedSize must be within the supported resolutions.");

class VideoDecoderPipelineTest
    : public testing::TestWithParam<DecoderPipelineTestParams> {
 public:
  VideoDecoderPipelineTest()
      : config_(VideoCodec::kVP8,
                VP8PROFILE_ANY,
                VideoDecoderConfig::AlphaMode::kIsOpaque,
                VideoColorSpace(),
                kNoTransformation,
                kCodedSize,
                gfx::Rect(kCodedSize),
                kCodedSize,
                EmptyExtraData(),
                EncryptionScheme::kUnencrypted) {
    auto pool = std::make_unique<MockVideoFramePool>();
    pool_ = pool.get();
    decoder_ = base::WrapUnique(new VideoDecoderPipeline(
        VideoDecoderPipeline::DecoderReservation::Take(
            std::numeric_limits<int>::max()),
        gpu::GpuDriverBugWorkarounds(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), std::move(pool),
        FakeFrameResourceConverter::Create(),
        VideoDecoderPipeline::DefaultPreferredRenderableFourccs(),
        std::make_unique<MockMediaLog>(),
        // This callback needs to be configured in the individual tests.
        base::BindOnce(&VideoDecoderPipelineTest::CreateNullMockDecoder),
        /*uses_oop_video_decoder=*/false,
        /*in_video_decoder_process=*/true));

    SetSupportedVideoDecoderConfigs({
        SupportedVideoDecoderConfig(
            /*profile_min,=*/VP8PROFILE_ANY,
            /*profile_max=*/VP9PROFILE_PROFILE0, kMinSupportedResolution,
            kMaxSupportedResolution,
            /*allow_encrypted=*/true,
            /*require_encrypted=*/false),
    });
  }
  ~VideoDecoderPipelineTest() override = default;

  void TearDown() override {
    pool_ = nullptr;
    VideoDecoderPipeline::DestroyAsync(std::move(decoder_));
    task_environment_.RunUntilIdle();
  }
  MOCK_METHOD1(OnInit, void(DecoderStatus));
  MOCK_METHOD1(OnOutput, void(scoped_refptr<VideoFrame>));
  MOCK_METHOD0(OnResetDone, void());
  MOCK_METHOD1(OnDecodeDone, void(DecoderStatus));
  MOCK_METHOD1(OnWaiting, void(WaitingReason));

  void SetCreateDecoderFunctionCB(VideoDecoderPipeline::CreateDecoderFunctionCB
                                      function) NO_THREAD_SAFETY_ANALYSIS {
    decoder_->create_decoder_function_cb_ = std::move(function);
  }

  void SetCreateImageProcessorCBForTesting(
      VideoDecoderPipeline::CreateImageProcessorCBForTesting function)
      NO_THREAD_SAFETY_ANALYSIS {
    decoder_->create_image_processor_cb_for_testing_ = std::move(function);
  }

  // Constructs |decoder_| with a given |create_decoder_function_cb| and
  // verifying |status_code| is received back in OnInit().
  void InitializeDecoder(
      VideoDecoderPipeline::CreateDecoderFunctionCB create_decoder_function_cb,
      DecoderStatus::Codes status_code,
      CdmContext* cdm_context = nullptr) {
    SetCreateDecoderFunctionCB(std::move(create_decoder_function_cb));

    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnInit(MatchesStatusCode(status_code)))
        .WillOnce(RunClosure(run_loop.QuitClosure()));

    decoder_->Initialize(
        config_, false /* low_delay */, cdm_context,
        base::BindOnce(&VideoDecoderPipelineTest::OnInit,
                       base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnOutput,
                            base::Unretained(this)),
        base::BindRepeating(&VideoDecoderPipelineTest::OnWaiting,
                            base::Unretained(this)));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(this);
  }

  static std::unique_ptr<VideoDecoderMixin> CreateNullMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    return nullptr;
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kOk);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns ok and
  // also indicates that it requires transcryption.
  static std::unique_ptr<VideoDecoderMixin> CreateGoodMockTranscryptDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kOk);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(true));
    return std::move(decoder);
  }

  // Creates a MockDecoder with an EXPECT_CALL on Initialize that returns error.
  static std::unique_ptr<VideoDecoderMixin> CreateBadMockDecoder(
      std::unique_ptr<MediaLog> /* media_log */,
      scoped_refptr<base::SequencedTaskRunner> /* decoder_task_runner */,
      base::WeakPtr<VideoDecoderMixin::Client> /* client */) {
    std::unique_ptr<MockDecoder> decoder(new MockDecoder());
    EXPECT_CALL(*decoder, Initialize(_, _, _, _, _, _))
        .WillOnce(::testing::WithArgs<3>([](VideoDecoder::InitCB init_cb) {
          std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
        }));
    EXPECT_CALL(*decoder, NeedsTranscryption()).WillRepeatedly(Return(false));
    return std::move(decoder);
  }

  VideoDecoderMixin* GetUnderlyingDecoder() NO_THREAD_SAFETY_ANALYSIS {
    return decoder_->decoder_.get();
  }

  void SetSupportedVideoDecoderConfigs(
      const SupportedVideoDecoderConfigs& configs) {
    decoder_->supported_configs_for_testing_ = configs;
  }

  void DetachDecoderSequenceChecker() NO_THREAD_SAFETY_ANALYSIS {
    // |decoder_| will be destroyed on its |decoder_task_runner| via
    // DestroyAsync(). This will trip its |decoder_sequence_checker_| if it has
    // been pegged to the test task runner, e.g. in PickDecoderOutputFormat().
    // Since in that case we don't care about threading, just detach it.
    DETACH_FROM_SEQUENCE(decoder_->decoder_sequence_checker_);

    if (decoder_->image_processor_) {
      // |decoder_->image_processor_->sequence_checker_| is pegged to the test
      // task runner because |decoder_->image_processor_| is created when we
      // call PickDecoderOutputFormat() from test code.
      // |decoder_->image_processor_| is then destroyed on the decoder task
      // runner because of VideoDecoderPipeline::DestroyAsync(). Thus the need
      // for this detachment.
      DETACH_FROM_SEQUENCE(decoder_->image_processor_->sequence_checker_);
    }
  }

  void InvokeWaitingCB(WaitingReason reason) {
    decoder_->decoder_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VideoDecoderPipeline::OnDecoderWaiting,
                                  base::Unretained(decoder_.get()), reason));
  }

  bool DecoderHasImageProcessor() NO_THREAD_SAFETY_ANALYSIS {
    return !!decoder_->image_processor_;
  }

  scoped_refptr<base::SequencedTaskRunner> GetDecoderTaskRunner()
      NO_THREAD_SAFETY_ANALYSIS {
    return decoder_->decoder_task_runner_;
  }

  base::test::TaskEnvironment task_environment_;
  VideoDecoderConfig config_;

  std::unique_ptr<VideoDecoderPipeline> decoder_;
  raw_ptr<MockVideoFramePool> pool_;
};

// Verifies the status code for several typical CreateDecoderFunctionCB cases.
TEST_P(VideoDecoderPipelineTest, Initialize) {
  InitializeDecoder(base::BindOnce(GetParam().create_decoder_function_cb),
                    GetParam().status_code);

  EXPECT_EQ(GetParam().status_code == DecoderStatus::Codes::kOk,
            !!GetUnderlyingDecoder());
}

const std::vector<DecoderPipelineTestParams>& GetDecoderPipelineTestParams() {
  static const base::NoDestructor<std::vector<DecoderPipelineTestParams>> val(
      std::vector<DecoderPipelineTestParams>{
          // A CreateDecoderFunctionCB that fails to Create() (i.e. returns a
          // null Decoder)
          {base::BindRepeating(
               &VideoDecoderPipelineTest::CreateNullMockDecoder),
           DecoderStatus::Codes::kFailedToCreateDecoder},

          // A CreateDecoderFunctionCB that works fine, i.e. Create()s and
          // Initialize()s correctly.
          {base::BindRepeating(
               &VideoDecoderPipelineTest::CreateGoodMockDecoder),
           DecoderStatus::Codes::kOk},

          // A CreateDecoderFunctionCB that Create()s ok but fails to
          // Initialize()
          // correctly.
          {base::BindRepeating(&VideoDecoderPipelineTest::CreateBadMockDecoder),
           DecoderStatus::Codes::kFailed},
      });
  return *val;
}

INSTANTIATE_TEST_SUITE_P(All,
                         VideoDecoderPipelineTest,
                         testing::ValuesIn(GetDecoderPipelineTestParams()));

// Verifies that trying to Initialize() with a non-supported config fails.
TEST_F(VideoDecoderPipelineTest, InitializeFailsDueToNotSupportedConfig) {
  // Configure the supported configs to something that we know is not supported,
  // e.g. making the smallest supported resolution larger than the |config_|
  // we'll be requesting.
  SetSupportedVideoDecoderConfigs({SupportedVideoDecoderConfig(
      /*profile_min=*/config_.profile(),
      /*profile_max=*/config_.profile(),
      /*coded_size_min=*/config_.coded_size() + gfx::Size(1, 1),
      kMaxSupportedResolution,
      /*allow_encrypted=*/true,
      /*require_encrypted=*/false)});

  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kUnsupportedConfig);
}

// Verifies the Reset sequence.
TEST_F(VideoDecoderPipelineTest, Reset) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kOk);

  // When we call Reset(), we expect GetUnderlyingDecoder()'s Reset() method to
  // be called, and when that method Run()s its argument closure, then
  // OnResetDone() is expected to be called.

  base::RunLoop run_loop;
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));

  EXPECT_CALL(*this, OnResetDone())
      .WillOnce(RunClosure(run_loop.QuitClosure()));

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
}

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormat) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  constexpr uint64_t kModifier = ~DRM_FORMAT_MOD_LINEAR;

  const struct {
    std::vector<PixelLayoutCandidate> input_candidates;
    PixelLayoutCandidate expected_chosen_candidate;
  } test_vectors[] = {
      // Easy cases: one candidate that is supported, should be chosen.
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
      // Two candidates, both supported: pick as per implementation.
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      // Two candidates, only one supported, the supported one should be picked.
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::NV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::YV12), kSize, kModifier}},
      {{PixelLayoutCandidate{Fourcc(Fourcc::YU16), kSize, kModifier},
        PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}},
       PixelLayoutCandidate{Fourcc(Fourcc::P010), kSize, kModifier}}};

  for (const auto& test_vector : test_vectors) {
    const Fourcc& expected_fourcc =
        test_vector.expected_chosen_candidate.fourcc;
    const gfx::Size& expected_coded_size =
        test_vector.expected_chosen_candidate.size;
    std::vector<ColorPlaneLayout> planes(
        VideoFrame::NumPlanes(expected_fourcc.ToVideoPixelFormat()));
    EXPECT_CALL(
        *pool_,
        Initialize(expected_fourcc, expected_coded_size, kVisibleRect,
                   /*natural_size=*/kVisibleRect.size(),
                   /*max_num_frames=*/::testing::Gt(kNumCodecReferenceFrames),
                   /*use_protected=*/false,
                   /*use_linear_buffers=*/false))
        .WillOnce(Return(*GpuBufferLayout::Create(
            expected_fourcc, expected_coded_size, std::move(planes),
            /*modifier=*/kModifier)));
    auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
        test_vector.input_candidates, kVisibleRect,
        /*decoder_natural_size=*/kVisibleRect.size(),
        /*output_size=*/std::nullopt,
        /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
        /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);
    ASSERT_TRUE(status_or_chosen_candidate.has_value());
    const PixelLayoutCandidate chosen_candidate =
        std::move(status_or_chosen_candidate).value();
    EXPECT_EQ(test_vector.expected_chosen_candidate, chosen_candidate)
        << " expected: "
        << test_vector.expected_chosen_candidate.fourcc.ToString()
        << ", actual: " << chosen_candidate.fourcc.ToString();
    EXPECT_FALSE(DecoderHasImageProcessor());
    testing::Mock::VerifyAndClearExpectations(pool_);
  }
  DetachDecoderSequenceChecker();
}

// These tests only work on non-linux vaapi systems, since on linux there is no
// support for different modifiers.
#if BUILDFLAG(USE_VAAPI) && !BUILDFLAG(IS_LINUX)

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected when the pool returns linear buffers. It should allocate an image
// processor in those cases.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormatLinearModifier) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  const Fourcc kFourcc(Fourcc::NV12);

  auto image_processor =
      std::make_unique<MockImageProcessor>(GetDecoderTaskRunner());
  ImageProcessorBackend::PortConfig port_config(
      Fourcc(Fourcc::NV12), gfx::Size(320, 240), {}, gfx::Rect(320, 240), {});
  EXPECT_CALL(*image_processor, input_config())
      .WillRepeatedly(testing::ReturnRef(port_config));
  EXPECT_CALL(*image_processor, output_config())
      .WillRepeatedly(testing::ReturnRef(port_config));

  base::MockCallback<VideoDecoderPipeline::CreateImageProcessorCBForTesting>
      image_processor_cb;
  EXPECT_CALL(image_processor_cb, Run(_, _, kSize, _))
      .WillOnce(Return(testing::ByMove(std::move(image_processor))));
  SetCreateImageProcessorCBForTesting(image_processor_cb.Get());

  // Modifier should be the linear format.
  GpuBufferLayout gpu_buffer_layout = *GpuBufferLayout::Create(
      kFourcc, kSize,
      std::vector<ColorPlaneLayout>(
          VideoFrame::NumPlanes(kFourcc.ToVideoPixelFormat())),
      /*modifier=*/DRM_FORMAT_MOD_LINEAR);
  EXPECT_CALL(*pool_, Initialize(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(gpu_buffer_layout));

  PixelLayoutCandidate candidate{Fourcc(Fourcc::NV12), kSize,
                                 /*modifier=*/~DRM_FORMAT_MOD_LINEAR};
  auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
      {candidate}, kVisibleRect,
      /*decoder_natural_size=*/kVisibleRect.size(),
      /*output_size=*/std::nullopt,
      /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
      /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);

  EXPECT_TRUE(status_or_chosen_candidate.has_value());
  // Main concern is that the image processor was set.
  EXPECT_TRUE(DecoderHasImageProcessor());
  DetachDecoderSequenceChecker();
}

// Verifies the algorithm for choosing formats in PickDecoderOutputFormat works
// as expected when the frame pool returns buffers that have an unsupported
// modifier.
TEST_F(VideoDecoderPipelineTest, PickDecoderOutputFormatUnsupportedModifier) {
  constexpr gfx::Size kSize(320, 240);
  constexpr gfx::Rect kVisibleRect(320, 240);
  constexpr size_t kNumCodecReferenceFrames = 4u;
  const Fourcc kFourcc(Fourcc::NV12);

  // Modifier is *not* the linear format.
  GpuBufferLayout gpu_buffer_layout = *GpuBufferLayout::Create(
      kFourcc, kSize,
      std::vector<ColorPlaneLayout>(
          VideoFrame::NumPlanes(kFourcc.ToVideoPixelFormat())),
      /*modifier=*/~DRM_FORMAT_MOD_LINEAR);
  EXPECT_CALL(*pool_, Initialize(_, _, _, _, _, _, _))
      .WillRepeatedly(Return(gpu_buffer_layout));

  // Make sure the modifier mismatches the |gpu_buffer_layout|'s
  constexpr uint64_t modifier = ~DRM_FORMAT_MOD_LINEAR + 1;
  PixelLayoutCandidate candidate{Fourcc(Fourcc::NV12), kSize, modifier};
  auto status_or_chosen_candidate = decoder_->PickDecoderOutputFormat(
      {candidate}, kVisibleRect,
      /*decoder_natural_size=*/kVisibleRect.size(),
      /*output_size=*/std::nullopt,
      /*num_codec_reference_frames=*/kNumCodecReferenceFrames,
      /*use_protected=*/false, /*need_aux_frame_pool=*/false, std::nullopt);

  EXPECT_FALSE(status_or_chosen_candidate.has_value());
  EXPECT_FALSE(DecoderHasImageProcessor());
  DetachDecoderSequenceChecker();
}

#endif  // BUILDFLAG(USE_VAAPI) && !BUILDFLAG(IS_LINUX)

// Verifies that ReleaseAllFrames is called on the frame pool when we receive
// the kDecoderStateLost event through the waiting callback. This can occur
// during protected content playback on Intel.
TEST_F(VideoDecoderPipelineTest, RebuildFramePoolsOnStateLost) {
  InitializeDecoder(
      base::BindOnce(&VideoDecoderPipelineTest::CreateGoodMockDecoder),
      DecoderStatus::Codes::kOk);

  // Simulate the waiting callback from the decoder for kDecoderStateLost.
  EXPECT_CALL(*this, OnWaiting(media::WaitingReason::kDecoderStateLost));
  InvokeWaitingCB(media::WaitingReason::kDecoderStateLost);
  task_environment_.RunUntilIdle();

  // Invoke Reset() as a client would do, and we then expect that to invoke the
  // method to rebuild the frame pool.
  EXPECT_CALL(*reinterpret_cast<MockDecoder*>(GetUnderlyingDecoder()), Reset(_))
      .WillOnce(::testing::WithArgs<0>(
          [](base::OnceClosure closure) { std::move(closure).Run(); }));
  EXPECT_CALL(*this, OnResetDone());
  EXPECT_CALL(*pool_, ReleaseAllFrames());

  decoder_->Reset(base::BindOnce(&VideoDecoderPipelineTest::OnResetDone,
                                 base::Unretained(this)));
  task_environment_.RunUntilIdle();
}
}  // namespace media
