// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_change/password_change_page_stability_waiter.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"

namespace {

using ::testing::_;

using page_content_annotations::mojom::PageStabilityMonitor;

class MockPageStabilityMonitor : public PageStabilityMonitor {
 public:
  MockPageStabilityMonitor() = default;
  ~MockPageStabilityMonitor() override = default;

  MOCK_METHOD(void,
              NotifyWhenStable,
              (base::TimeDelta, NotifyWhenStableCallback),
              (override));

  void Bind(mojo::PendingReceiver<PageStabilityMonitor> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  void Close() { receiver_.reset(); }

 private:
  mojo::Receiver<PageStabilityMonitor> receiver_{this};
};

class MockPageStabilityMonitorManager
    : public page_content_annotations::mojom::PageStabilityMonitorManager {
 public:
  MockPageStabilityMonitorManager() = default;
  ~MockPageStabilityMonitorManager() override = default;

  void BindPendingReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(
        this,
        mojo::PendingAssociatedReceiver<
            page_content_annotations::mojom::PageStabilityMonitorManager>(
            std::move(handle)));
  }

  MOCK_METHOD(void,
              CreatePageStabilityMonitor,
              (mojo::PendingReceiver<PageStabilityMonitor>, bool),
              (override));

 private:
  mojo::AssociatedReceiverSet<
      page_content_annotations::mojom::PageStabilityMonitorManager>
      receivers_;
};

}  // namespace

class PasswordChangePageStabilityWaiterTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PasswordChangePageStabilityWaiterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    scoped_feature_list_.InitAndDisableFeature(
        password_manager::features::kUseDetachedWidget);
  }
  ~PasswordChangePageStabilityWaiterTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    NavigateAndCommit(GURL("https://example.com"));

    blink::AssociatedInterfaceProvider* remote_interfaces =
        web_contents()->GetPrimaryMainFrame()->GetRemoteAssociatedInterfaces();
    remote_interfaces->OverrideBinderForTesting(
        page_content_annotations::mojom::PageStabilityMonitorManager::Name_,
        base::BindRepeating(
            &MockPageStabilityMonitorManager::BindPendingReceiver,
            base::Unretained(&mock_monitor_manager_)));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockPageStabilityMonitorManager mock_monitor_manager_;
  MockPageStabilityMonitor mock_page_stability_monitor_;
  password_manager::StubPasswordManagerClient stub_client_;
};

TEST_F(PasswordChangePageStabilityWaiterTest, PageBecomesStable) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_monitor_manager_, CreatePageStabilityMonitor)
      .WillOnce([&](mojo::PendingReceiver<PageStabilityMonitor> receiver, bool) {
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback;
  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable(_, _))
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback callback) {
        monitor_callback = std::move(callback);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback; }));
  std::move(monitor_callback).Run();
  EXPECT_TRUE(future.Wait());
}

TEST_F(PasswordChangePageStabilityWaiterTest, RestartsOnNavigation) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_monitor_manager_, CreatePageStabilityMonitor)
      .Times(2)
      .WillRepeatedly([&](mojo::PendingReceiver<PageStabilityMonitor> receiver, bool) {
        mock_page_stability_monitor_.Close();
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback1;
  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback2;

  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable)
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback1 = std::move(cb);
      })
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback2 = std::move(cb);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback1; }));

  // Emulate navigation event.
  waiter.DidFinishNavigation(nullptr);
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback2; }));
  std::move(monitor_callback2).Run();
  EXPECT_TRUE(future.Wait());
}

TEST_F(PasswordChangePageStabilityWaiterTest, MonitorDisconnects) {
  base::test::TestFuture<void> future;

  EXPECT_CALL(mock_monitor_manager_, CreatePageStabilityMonitor)
      .WillOnce([&](mojo::PendingReceiver<PageStabilityMonitor> receiver, bool) {
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback;
  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable)
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback callback) {
        monitor_callback = std::move(callback);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback; }));

  mock_page_stability_monitor_.Close();
  EXPECT_TRUE(future.Wait());
}

// TODO(crbug.com/517949256): Disabled on Linux due to excessive flakiness.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_DisconnectDuringNavigation DISABLED_DisconnectDuringNavigation
#else
#define MAYBE_DisconnectDuringNavigation DisconnectDuringNavigation
#endif
TEST_F(PasswordChangePageStabilityWaiterTest,
       MAYBE_DisconnectDuringNavigation) {
  base::test::TestFuture<void> future;

  // We expect two calls to CreatePageStabilityMonitor: one for the initial
  // page and one after the navigation completes.
  EXPECT_CALL(mock_monitor_manager_, CreatePageStabilityMonitor)
      .Times(2)
      .WillRepeatedly([&](mojo::PendingReceiver<PageStabilityMonitor> receiver, bool) {
        mock_page_stability_monitor_.Close();
        mock_page_stability_monitor_.Bind(std::move(receiver));
      });

  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback1;
  PageStabilityMonitor::NotifyWhenStableCallback monitor_callback2;

  EXPECT_CALL(mock_page_stability_monitor_, NotifyWhenStable)
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback1 = std::move(cb);
      })
      .WillOnce([&](base::TimeDelta,
                    PageStabilityMonitor::NotifyWhenStableCallback cb) {
        monitor_callback2 = std::move(cb);
      });

  PasswordChangePageStabilityWaiter waiter(web_contents(), &stub_client_,
                                           future.GetCallback());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback1; }));

  // Emulate a navigation starting.
  waiter.DidStartNavigation(nullptr);

  // Close the monitor to simulate disconnect due to navigation.
  mock_page_stability_monitor_.Close();

  // Spin the message loop to allow Mojo disconnect handlers to run.
  base::RunLoop().RunUntilIdle();

  // The callback should NOT have run yet since we are in the middle of a
  // navigation.
  EXPECT_FALSE(future.IsReady());

  // Emulate navigation completion.
  waiter.DidFinishNavigation(nullptr);

  // A new monitor should be created.
  EXPECT_TRUE(base::test::RunUntil([&]() { return !!monitor_callback2; }));

  // Resolve the new monitor.
  std::move(monitor_callback2).Run();

  // Now the waiter should complete.
  EXPECT_TRUE(future.Wait());
}
