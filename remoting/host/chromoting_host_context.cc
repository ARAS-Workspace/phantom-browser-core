// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/message_loop/message_pump_type.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/ssl/client_cert_store.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/certificate_helpers.h"
#include "remoting/base/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace remoting {

namespace {

void DisallowBlockingOperations() {
  base::DisallowBlocking();
  // TODO(crbug.com/41360128): Re-enable after the underlying issue is fixed.
  // base::DisallowBaseSyncPrimitives();
}

class ChromotingHostContextDesktop : public ChromotingHostContext {
 public:
  ChromotingHostContextDesktop(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<AutoThreadTaskRunner> input_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_request_context_getter);

  ChromotingHostContextDesktop(const ChromotingHostContextDesktop&) = delete;
  ChromotingHostContextDesktop& operator=(const ChromotingHostContextDesktop&) =
      delete;

  ~ChromotingHostContextDesktop() override;

  // remoting::ChromotingHostContext implementation.
  std::unique_ptr<ChromotingHostContext> Copy() override;
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore() const override;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter()
      const override;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() override;
  CreateClientCertStoreCallback create_client_cert_store_callback()
      const override;

 private:
  // Serves URLRequestContexts that use the network and UI task runners.
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;

  // Makes a SharedURLLoaderFactory out of |url_request_context_getter_|
  std::unique_ptr<network::TransitionalURLLoaderFactoryOwner>
      url_loader_factory_owner_;
};

ChromotingHostContextDesktop::ChromotingHostContextDesktop(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : ChromotingHostContext(ui_task_runner,
                            file_task_runner,
                            input_task_runner,
                            network_task_runner,
                            video_capture_task_runner),
      url_request_context_getter_(url_request_context_getter) {}

ChromotingHostContextDesktop::~ChromotingHostContextDesktop() {
  if (url_loader_factory_owner_) {
    network_task_runner()->DeleteSoon(FROM_HERE,
                                      url_loader_factory_owner_.release());
  }
}

std::unique_ptr<ChromotingHostContext> ChromotingHostContextDesktop::Copy() {
  return std::make_unique<ChromotingHostContextDesktop>(
      ui_task_runner(), file_task_runner(), input_task_runner(),
      network_task_runner(), video_capture_task_runner(),
      url_request_context_getter_);
}

std::unique_ptr<net::ClientCertStore>
ChromotingHostContextDesktop::CreateClientCertStore() const {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  return CreateClientCertStoreInstance();
}

scoped_refptr<net::URLRequestContextGetter>
ChromotingHostContextDesktop::url_request_context_getter() const {
  return url_request_context_getter_;
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromotingHostContextDesktop::url_loader_factory() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  if (!url_loader_factory_owner_) {
    url_loader_factory_owner_ =
        std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
            url_request_context_getter_, /* is_trusted= */ true);
  }
  return url_loader_factory_owner_->GetURLLoaderFactory();
}

ChromotingHostContext::CreateClientCertStoreCallback
ChromotingHostContextDesktop::create_client_cert_store_callback() const {
  return base::BindRepeating(&CreateClientCertStoreInstance);
}


}  // namespace

ChromotingHostContext::ChromotingHostContext(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner)
    : ui_task_runner_(ui_task_runner),
      file_task_runner_(file_task_runner),
      input_task_runner_(input_task_runner),
      network_task_runner_(network_task_runner),
      video_capture_task_runner_(video_capture_task_runner) {}

ChromotingHostContext::~ChromotingHostContext() = default;


scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::file_task_runner()
    const {
  return file_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::input_task_runner()
    const {
  return input_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::network_task_runner()
    const {
  return network_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner> ChromotingHostContext::ui_task_runner()
    const {
  return ui_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::video_capture_task_runner() const {
  return video_capture_task_runner_;
}

policy::ManagementService* ChromotingHostContext::management_service() {
  return policy::PlatformManagementService::GetInstance();
}

std::unique_ptr<ChromotingHostContext> ChromotingHostContext::Create(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
  scoped_refptr<AutoThreadTaskRunner> file_task_runner =
      AutoThread::CreateWithType("ChromotingFileThread", ui_task_runner,
                                 base::MessagePumpType::IO);

  scoped_refptr<AutoThreadTaskRunner> network_task_runner =
      AutoThread::CreateWithType("ChromotingNetworkThread", ui_task_runner,
                                 base::MessagePumpType::IO);
  network_task_runner->PostTask(FROM_HERE,
                                base::BindOnce(&DisallowBlockingOperations));

  // InputInjectorX11 requires an X11EventSource, which can only be created
  // on a UI thread.
  scoped_refptr<AutoThreadTaskRunner> input_task_runner =
      AutoThread::CreateWithType("ChromotingInputThread", ui_task_runner,
#if BUILDFLAG(IS_LINUX)
                                 base::MessagePumpType::UI);
#else
                                 base::MessagePumpType::IO);
#endif  // BUILDFLAG(IS_LINUX)

  return std::make_unique<ChromotingHostContextDesktop>(
      ui_task_runner, file_task_runner, input_task_runner, network_task_runner,
#if BUILDFLAG(IS_APPLE)
      // Mac requires a UI thread for the capturer.
      AutoThread::CreateWithType("ChromotingCaptureThread", ui_task_runner,
                                 base::MessagePumpType::UI),
#else   // !BUILDFLAG(IS_APPLE)
      AutoThread::Create("ChromotingCaptureThread", ui_task_runner),
#endif  // !BUILDFLAG(IS_APPLE)
      base::MakeRefCounted<URLRequestContextGetter>(network_task_runner));
}

// static
std::unique_ptr<ChromotingHostContext> ChromotingHostContext::CreateForTesting(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  return ChromotingHostContext::Create(ui_task_runner);
}

}  // namespace remoting
