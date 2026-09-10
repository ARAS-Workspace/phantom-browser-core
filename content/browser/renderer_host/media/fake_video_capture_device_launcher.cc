// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/fake_video_capture_device_launcher.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/token.h"
#include "content/public/browser/browser_context.h"
#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/capture/video/video_capture_buffer_pool_impl.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "media/capture/video/video_frame_receiver_on_task_runner.h"

namespace {

class FakeLaunchedVideoCaptureDevice
    : public content::LaunchedVideoCaptureDevice {
 public:
  explicit FakeLaunchedVideoCaptureDevice(
      std::unique_ptr<media::VideoCaptureDevice> device)
      : device_(std::move(device)) {}

  ~FakeLaunchedVideoCaptureDevice() override { device_->StopAndDeAllocate(); }

  void GetPhotoState(
      media::VideoCaptureDevice::GetPhotoStateCallback callback) override {
    device_->GetPhotoState(std::move(callback));
  }
  void SetPhotoOptions(
      media::mojom::PhotoSettingsPtr settings,
      media::VideoCaptureDevice::SetPhotoOptionsCallback callback) override {
    device_->SetPhotoOptions(std::move(settings), std::move(callback));
  }
  void TakePhoto(
      media::VideoCaptureDevice::TakePhotoCallback callback) override {
    device_->TakePhoto(std::move(callback));
  }
  void MaybeSuspendDevice() override { device_->MaybeSuspend(); }
  void ResumeDevice() override { device_->Resume(); }
  void ApplySubCaptureTarget(
      media::mojom::SubCaptureTargetType type,
      const base::Token& target,
      uint32_t sub_capture_target_version,
      base::OnceCallback<void(media::mojom::ApplySubCaptureTargetResult)>
          callback) override {
    device_->ApplySubCaptureTarget(type, target, sub_capture_target_version,
                                   std::move(callback));
  }
  void RequestRefreshFrame() override { device_->RequestRefreshFrame(); }
  void SetDesktopCaptureWindowIdAsync(gfx::NativeViewId window_id,
                                      base::OnceClosure done_cb) override {
    // Do nothing.
  }
  void OnUtilizationReport(media::VideoCaptureFeedback feedback) override {
    device_->OnUtilizationReport(feedback);
  }

 private:
  std::unique_ptr<media::VideoCaptureDevice> device_;
};

}  // anonymous namespace

namespace content {

FakeVideoCaptureDeviceLauncher::FakeVideoCaptureDeviceLauncher(
    media::VideoCaptureSystem* system)
    : system_(system) {
  DCHECK(system_);
}

FakeVideoCaptureDeviceLauncher::~FakeVideoCaptureDeviceLauncher() = default;

void FakeVideoCaptureDeviceLauncher::LaunchDeviceAsync(
    const std::string& device_id,
    blink::mojom::MediaStreamType stream_type,
    const media::VideoCaptureParams& params,
    base::WeakPtr<media::VideoFrameReceiver> receiver,
    base::OnceClosure connection_lost_cb,
    Callbacks* callbacks,
    base::OnceClosure done_cb) {
  media::VideoCaptureErrorOrDevice device_or_error =
      system_->CreateDevice(device_id);
  if (!device_or_error.ok()) {
    callbacks->OnDeviceLaunchFailed(device_or_error.error());
    std::move(done_cb).Run();
    return;
  }
  std::unique_ptr<media::VideoCaptureDevice> device =
      device_or_error.ReleaseDevice();
  auto buffer_pool = base::MakeRefCounted<media::VideoCaptureBufferPoolImpl>(
      media::VideoCaptureBufferType::kSharedMemory);
  auto device_client = std::make_unique<media::VideoCaptureDeviceClient>(
      std::make_unique<media::VideoFrameReceiverOnTaskRunner>(
          receiver, base::SingleThreadTaskRunner::GetCurrentDefault()),
      std::move(buffer_pool));
  device->AllocateAndStart(params, std::move(device_client));
  auto launched_device =
      std::make_unique<FakeLaunchedVideoCaptureDevice>(std::move(device));
  callbacks->OnDeviceLaunched(std::move(launched_device));
}

void FakeVideoCaptureDeviceLauncher::AbortLaunch() {
  // Do nothing.
}

}  // namespace content
