// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/network_connection_change_simulator.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/network_service_util.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace content {

// SetConnectionType will block until the network connection changes, and
// unblocking it involves posting a task (see
// NetworkConnectionTracker::OnNetworkChanged). If SetConnectionType is ever
// called downstream of a task run within another RunLoop::Run call, this
// class's RunLoop::Run will deadlock because the task needed to unblock it
// won't be run. To stop this, this class uses RunLoops that allow nested tasks.
constexpr base::RunLoop::Type kRunLoopType =
    base::RunLoop::Type::kNestableTasksAllowed;

NetworkConnectionChangeSimulator::NetworkConnectionChangeSimulator() = default;
NetworkConnectionChangeSimulator::~NetworkConnectionChangeSimulator() = default;

void NetworkConnectionChangeSimulator::SetConnectionType(
    net::NetworkChangeNotifier::ConnectionType type) {
  network::NetworkConnectionTracker* network_connection_tracker =
      content::GetNetworkConnectionTracker();
  net::NetworkChangeNotifier::ConnectionType connection_type =
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN;
  run_loop_ = std::make_unique<base::RunLoop>(kRunLoopType);
  network_connection_tracker->AddNetworkConnectionObserver(this);
  SimulateNetworkChange(type);
  // Make sure the underlying network connection type becomes |type|.
  // The while loop is necessary because in some machine such as "Builder
  // linux64 trunk", the |connection_type| can be CONNECTION_ETHERNET before
  // it changes to |type|. So here it needs to wait until the
  // |connection_type| becomes |type|.
  while (
      !network_connection_tracker->GetConnectionType(
          &connection_type,
          base::BindOnce(&NetworkConnectionChangeSimulator::OnConnectionChanged,
                         base::Unretained(this))) ||
      connection_type != type) {
    SimulateNetworkChange(type);
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }
  network_connection_tracker->RemoveNetworkConnectionObserver(this);
}

// static
void NetworkConnectionChangeSimulator::SimulateNetworkChange(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (IsOutOfProcessNetworkService()) {
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test;
    content::GetNetworkService()->BindTestInterfaceForTesting(
        network_service_test.BindNewPipeAndPassReceiver());
    base::RunLoop run_loop(kRunLoopType);
    network_service_test->SimulateNetworkChange(type, run_loop.QuitClosure());
    run_loop.Run();
    return;
  }
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(type);
}

void NetworkConnectionChangeSimulator::OnConnectionChanged(
    net::NetworkChangeNotifier::ConnectionType connection_type) {
  DCHECK(run_loop_);
  run_loop_->Quit();
}

}  // namespace content
