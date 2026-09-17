// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"

#if (!BUILDFLAG(IS_ANDROID))
namespace {
class TestDevToolsClient : public ::content::DevToolsAgentHostClient {
 public:
  TestDevToolsClient() = default;
  ~TestDevToolsClient() override { Detach(); }

  void WaitForInvokedEvent() {
    ASSERT_TRUE(
        base::test::RunUntil([this]() { return !invoked_events_.empty(); }));
  }

  void WaitForRespondedEvent() {
    ASSERT_TRUE(
        base::test::RunUntil([this]() { return !responded_events_.empty(); }));
  }

  void AttachToAndEnableWebMCP(
      scoped_refptr<content::DevToolsAgentHost> agent_host) {
    agent_host_ = agent_host;
    agent_host_->AttachClient(this);

    const std::string enable_message =
        R"JSON({"id": 1, "method": "WebMCP.enable"})JSON";
    agent_host_->DispatchProtocolMessage(this,
                                         base::as_byte_span(enable_message));
  }

  void Detach() {
    if (agent_host_) {
      agent_host_->DetachClient(this);
      agent_host_ = nullptr;
    }
  }

  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    std::string_view message_str(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    std::optional<base::Value> parsed = base::test::ParseJson(message_str);
    if (!parsed || !parsed->is_dict()) {
      return;
    }

    const base::DictValue& dict = parsed->GetDict();

    const std::string* method = dict.FindString("method");
    if (!method) {
      return;
    }

    if (*method == "WebMCP.toolInvoked") {
      invoked_events_.push_back(std::move(*parsed));
    } else if (*method == "WebMCP.toolResponded") {
      responded_events_.push_back(std::move(*parsed));
    }
  }

  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}

  const std::vector<base::Value>& invoked_events() const {
    return invoked_events_;
  }
  const std::vector<base::Value>& responded_events() const {
    return responded_events_;
  }

 private:
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::vector<base::Value> invoked_events_;
  std::vector<base::Value> responded_events_;
};

}  // namespace

#endif  // !BUILDFLAG(IS_ANDROID)
