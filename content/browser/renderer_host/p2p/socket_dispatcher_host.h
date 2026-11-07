// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/network_anonymization_key.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "services/network/public/mojom/p2p_trusted.mojom.h"

namespace content {

// Responsible for P2P sockets. Lives on the UI thread.
class P2PSocketDispatcherHost
    : public network::mojom::P2PTrustedSocketManagerClient {
 public:
  explicit P2PSocketDispatcherHost(int render_process_id);

  P2PSocketDispatcherHost(const P2PSocketDispatcherHost&) = delete;
  P2PSocketDispatcherHost& operator=(const P2PSocketDispatcherHost&) = delete;

  ~P2PSocketDispatcherHost() override;

  void BindReceiver(
      RenderProcessHostImpl& process,
      mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver,
      net::NetworkAnonymizationKey anonymization_key,
      const GlobalRenderFrameHostId& render_frame_host_id);

  base::WeakPtr<P2PSocketDispatcherHost> GetWeakPtr();
  void PauseSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& frame_id);
  void ResumeSocketManagerForRenderFrameHost(
      const GlobalRenderFrameHostId& frame_id);

 private:
  // network::mojom::P2PTrustedSocketManagerClient overrides:
  void InvalidSocketPortRangeRequested() override;

  int render_process_id_;

  mojo::ReceiverSet<network::mojom::P2PTrustedSocketManagerClient> receivers_;
  mojo::RemoteSet<network::mojom::P2PTrustedSocketManager>
      trusted_socket_managers_;

  base::flat_map<GlobalRenderFrameHostId, mojo::RemoteSetElementId>
      frame_host_to_socket_manager_id_;

  mojo::Remote<network::mojom::P2PNetworkNotificationClient>
      network_notification_client_;

  base::WeakPtrFactory<P2PSocketDispatcherHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_P2P_SOCKET_DISPATCHER_HOST_H_
