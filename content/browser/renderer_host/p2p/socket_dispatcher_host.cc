// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/heap_array.h"
#include "base/functional/bind.h"
#include "content/browser/bad_message.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "services/network/public/mojom/network_context.mojom.h"

using content::BrowserThread;

namespace content {

P2PSocketDispatcherHost::P2PSocketDispatcherHost(int render_process_id)
    : render_process_id_(render_process_id) {}

P2PSocketDispatcherHost::~P2PSocketDispatcherHost() = default;

void P2PSocketDispatcherHost::BindReceiver(
    RenderProcessHostImpl& process,
    mojo::PendingReceiver<network::mojom::P2PSocketManager> receiver,
    net::NetworkAnonymizationKey anonymization_key,
    const GlobalRenderFrameHostId& render_frame_host_id) {
  CHECK_EQ(process.GetDeprecatedID(), render_process_id_,
           base::NotFatalUntil::M153);

  mojo::PendingRemote<network::mojom::P2PTrustedSocketManagerClient>
      trusted_socket_manager_client;
  receivers_.Add(
      this, trusted_socket_manager_client.InitWithNewPipeAndPassReceiver());

  mojo::PendingRemote<network::mojom::P2PTrustedSocketManager>
      pending_trusted_socket_manager;
  process.GetStoragePartition()->GetNetworkContext()->CreateP2PSocketManager(
      anonymization_key, std::move(trusted_socket_manager_client),
      pending_trusted_socket_manager.InitWithNewPipeAndPassReceiver(),
      std::move(receiver));
  mojo::Remote<network::mojom::P2PTrustedSocketManager> trusted_socket_manager(
      std::move(pending_trusted_socket_manager));
  mojo::RemoteSetElementId manager_id =
      trusted_socket_managers_.Add(std::move(trusted_socket_manager));
  frame_host_to_socket_manager_id_.emplace(render_frame_host_id, manager_id);
}

void P2PSocketDispatcherHost::PauseSocketManagerForRenderFrameHost(
    const GlobalRenderFrameHostId& frame_id) {
  if (auto it = frame_host_to_socket_manager_id_.find(frame_id);
      it != frame_host_to_socket_manager_id_.end()) {
    mojo::RemoteSetElementId manager_id = it->second;
    if (trusted_socket_managers_.Contains(manager_id)) {
      trusted_socket_managers_.Get(manager_id)
          ->PauseNetworkChangeNotifications();
    }
  }
}
void P2PSocketDispatcherHost::ResumeSocketManagerForRenderFrameHost(
    const GlobalRenderFrameHostId& frame_id) {
  if (auto it = frame_host_to_socket_manager_id_.find(frame_id);
      it != frame_host_to_socket_manager_id_.end()) {
    mojo::RemoteSetElementId manager_id = it->second;
    if (trusted_socket_managers_.Contains(manager_id)) {
      trusted_socket_managers_.Get(manager_id)
          ->ResumeNetworkChangeNotifications();
    }
  }
}

base::WeakPtr<P2PSocketDispatcherHost> P2PSocketDispatcherHost::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void P2PSocketDispatcherHost::InvalidSocketPortRangeRequested() {
  bad_message::ReceivedBadMessage(render_process_id_,
                                  bad_message::SDH_INVALID_PORT_RANGE);
}

}  // namespace content
