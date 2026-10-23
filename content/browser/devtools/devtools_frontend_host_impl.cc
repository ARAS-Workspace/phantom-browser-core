// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_frontend_host_impl.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/strcat.h"
#include "base/strings/string_view_util.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/grit/devtools_resources_map.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/webui/resource_path.h"

namespace content {
namespace {
const char kCompatibilityScript[] = "devtools_compatibility.js";
const char kCompatibilityScriptSourceURL[] =
    "\n//# "
    "sourceURL=devtools://devtools/bundled/devtools_compatibility.js";
}

// static
std::unique_ptr<DevToolsFrontendHost> DevToolsFrontendHost::Create(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback) {
  DCHECK(!frame_host->GetParent());
  return std::make_unique<DevToolsFrontendHostImpl>(frame_host,
                                                    handle_message_callback);
}

// static
std::unique_ptr<DevToolsFrontendHostImpl>
DevToolsFrontendHostImpl::CreateForTesting(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback) {
  DCHECK(!frame_host->GetParent());
  return std::make_unique<DevToolsFrontendHostImpl>(frame_host,
                                                    handle_message_callback);
}

// static
void DevToolsFrontendHost::SetupExtensionsAPI(
    RenderFrameHost* frame_host,
    const std::string& extension_api) {
  DCHECK(frame_host->GetParent());
  mojo::AssociatedRemote<blink::mojom::DevToolsFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  frontend->SetupDevToolsExtensionAPI(extension_api);
}

// static
scoped_refptr<base::RefCountedMemory>
DevToolsFrontendHost::GetFrontendResourceBytes(std::string_view path) {
  for (const auto& [resource_path, id, filepath] : kDevtoolsResources) {
    if (path == resource_path) {
      return GetContentClient()->GetDataResourceBytes(id);
    }
  }
  return nullptr;
}

// static
std::string DevToolsFrontendHost::GetFrontendResource(std::string_view path) {
  scoped_refptr<base::RefCountedMemory> bytes = GetFrontendResourceBytes(path);
  if (!bytes)
    return std::string();
  return std::string(base::as_string_view(*bytes));
}

DevToolsFrontendHostImpl::DevToolsFrontendHostImpl(
    RenderFrameHost* frame_host,
    const HandleMessageCallback& handle_message_callback)
    : WebContentsObserver(WebContents::FromRenderFrameHost(frame_host)),
      handle_message_callback_(handle_message_callback) {
  mojo::AssociatedRemote<blink::mojom::DevToolsFrontend> frontend;
  frame_host->GetRemoteAssociatedInterfaces()->GetInterface(&frontend);
  std::string api_script =
      base::StrCat({GetFrontendResource(kCompatibilityScript),
                    kCompatibilityScriptSourceURL});
  frontend->SetupDevToolsFrontend(api_script,
                                  receiver_.BindNewEndpointAndPassRemote());
}

DevToolsFrontendHostImpl::~DevToolsFrontendHostImpl() = default;

void DevToolsFrontendHostImpl::BadMessageReceived() {
  if (!web_contents()) {
    return;
  }

  bad_message::ReceivedBadMessage(
      web_contents()->GetPrimaryMainFrame()->GetProcess(),
      bad_message::DFH_BAD_EMBEDDER_MESSAGE);
}

void DevToolsFrontendHostImpl::DispatchEmbedderMessage(
    base::DictValue message) {
  handle_message_callback_.Run(std::move(message));
}

}  // namespace content
