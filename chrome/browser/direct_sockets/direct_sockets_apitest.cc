// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/common/features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/direct_sockets_test_helpers.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/manifest_constants.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/extension_apitest.h"
#include "extensions/browser/api/sockets_udp/test_udp_echo_server.h"
#include "extensions/common/extension.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr char kHostname[] = "direct-sockets.com";

constexpr std::string_view kTcpReadWriteScript = R"(
  new Promise(async (resolve, reject) => {
    try {
      const socket = new TCPSocket($1, $2);

      const { readable, writable } = await socket.opened;

      const reader = readable.getReader();
      const writer = writable.getWriter();

      const kTcpPacket =
        "POST /echo HTTP/1.1\r\n" +
        "Content-Length: 19\r\n\r\n" +
        "0100000005320000005";

      // The echo server can send back the response in multiple chunks.
      // We must wait for at least `kTcpMinExpectedResponseLength` bytes to
      // be received before matching the response with `kTcpResponsePattern`.
      const kTcpMinExpectedResponseLength = 102;

      const kTcpResponsePattern = "0100000005320000005";

      let tcpResponse = "";
      const readUntil = async () => {
        const { value, done } = await reader.read();
        if (done) {
          reject("ReadableStream must not be exhausted at this point.");
        }

        const message = (new TextDecoder()).decode(value);
        tcpResponse += message;
        if (tcpResponse.length >= kTcpMinExpectedResponseLength) {
          if (!tcpResponse.match(kTcpResponsePattern)) {
            reject("The data returned must match the data sent.");
          }

          resolve();
        } else {
          readUntil();
        }
      };

      writer.write((new TextEncoder()).encode(kTcpPacket));

      readUntil();
    } catch (err) {
      reject(err);
    }
  });
)";

constexpr std::string_view kUdpConnectedReadWriteScript = R"(
  new Promise(async (resolve, reject) => {
    try {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
      const { readable, writable } = await socket.opened;

      const kUdpMessage = "udp_message";
      writable.getWriter().write({
        data: (new TextEncoder()).encode(kUdpMessage)
      });
      return await readable.getReader().read().then(packet => {
        const { value, done } = packet;
        if (done) {
          reject("ReadableStream must not be exhausted at this point.");
        }
        const { data } = value;
        if ((new TextDecoder()).decode(data) !== kUdpMessage) {
          reject("The data returned must match the data sent.");
        }
        resolve();
      });
    } catch (err) {
      reject(err);
    }
  });
)";

constexpr std::string_view kUdpBoundReadWriteScript = R"(
  new Promise(async (resolve, reject) => {
    try {
      const socket = new UDPSocket({ localAddress: "127.0.0.1" });
      const { readable, writable } = await socket.opened;

      const kUdpMessage = "udp_message";
      writable.getWriter().write({
        data: (new TextEncoder()).encode(kUdpMessage),
        remoteAddress: $1,
        remotePort: $2,
      });
      return await readable.getReader().read().then(packet => {
        const { value, done } = packet;
        if (done) {
          reject("ReadableStream must not be exhausted at this point.");
        }
        const { data, remoteAddress, remotePort } = value;
        if ((new TextDecoder()).decode(data) !== kUdpMessage) {
          reject("The data returned must match the data sent.");
        }
        if (remoteAddress !== "127.0.0.1") {
          reject(`Expected remoteAddress = 127.0.0.1, got ${remoteAddress}`);
        }
        if (remotePort !== $2) {
          reject(`Expected remotePort = $2, got ${remotePort}`);
        }
        resolve();
      });
    } catch (err) {
      reject(err);
    }
  });
)";

static constexpr std::string_view kTcpServerExchangePacketWithTcpScript = R"(
  new Promise(async (resolve, reject) => {
    const assertEq = (actual, expected) => {
      const jf = e => JSON.stringify(e);
      if (actual !== expected) {
        reject(`Expected ${jf(expected)}, got ${jf(actual)}`);
      }
    };

    const kPacket = "I'm a netcat. Meow-meow!";

    // |localPort| is intentionally omitted so that the OS will pick one itself.
    const serverSocket = new TCPServerSocket('127.0.0.1');
    const { localPort: serverSocketPort } = await serverSocket.opened;

    // Connect a client to the server.
    const clientSocket = new TCPSocket('127.0.0.1', serverSocketPort);

    async function acceptOnce() {
      const { readable } = await serverSocket.opened;
      const reader = readable.getReader();
      const { value: acceptedSocket, done } = await reader.read();
      assertEq(done, false);
      reader.releaseLock();
      return acceptedSocket;
    };

    const acceptedSocket = await acceptOnce();
    await clientSocket.opened;

    const encoder = new TextEncoder();
    const decoder = new TextDecoder();

    async function acceptedSocketSend() {
      const { writable } = await acceptedSocket.opened;
      const writer = writable.getWriter();

      await writer.ready;
      await writer.write(encoder.encode(kPacket));

      writer.releaseLock();
    }

    async function clientSocketReceive() {
      const { readable } = await clientSocket.opened;
      const reader = readable.getReader();
      let result = "";
      while (result.length < kPacket.length) {
        const { value, done } = await reader.read();
        assertEq(done, false);
        result += decoder.decode(value);
      }
      reader.releaseLock();
      assertEq(result, kPacket);
    }

    acceptedSocketSend();
    await clientSocketReceive();

    await clientSocket.close();
    await acceptedSocket.close();
    await serverSocket.close();

    resolve();
  });
)";

#if BUILDFLAG(ENABLE_EXTENSIONS)

base::DictValue GenerateManifest(
    std::optional<base::DictValue> socket_permissions = {}) {
  auto manifest = base::DictValue()
                      .Set(extensions::manifest_keys::kName,
                           "Direct Sockets in Chrome Apps")
                      .Set(extensions::manifest_keys::kManifestVersion, 2)
                      .Set(extensions::manifest_keys::kVersion, "1.0");

  manifest.SetByDottedPath(
      extensions::manifest_keys::kPlatformAppBackgroundScripts,
      base::ListValue().Append("background.js"));

  if (socket_permissions) {
    manifest.Set(extensions::manifest_keys::kSockets,
                 std::move(*socket_permissions));
  }

  return manifest;
}

auto AccessBlocked() {
  return testing::HasSubstr("Access to the requested host or port is blocked");
}

auto ErrorIs(const auto& matcher) {
  return content::EvalJsResult::ErrorIs(matcher);
}

#endif

class TestServer {
 public:
  virtual ~TestServer() = default;

  virtual void Start(network::mojom::NetworkContext* network_context) = 0;
  virtual void Stop() = 0;
  virtual uint16_t port() const = 0;
};

class TcpHttpTestServer : public TestServer {
 public:
  void Start(network::mojom::NetworkContext* network_context) override {
    DCHECK(!test_server_);
    test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTP);
    test_server_->AddDefaultHandlers();
    ASSERT_TRUE(test_server_->Start());
  }

  void Stop() override { test_server_.reset(); }

  uint16_t port() const override {
    DCHECK(test_server_);
    return test_server_->port();
  }

 private:
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

class UdpEchoTestServer : public TestServer {
 public:
  void Start(network::mojom::NetworkContext* network_context) override {
    DCHECK(!udp_echo_server_);
    udp_echo_server_ = std::make_unique<extensions::TestUdpEchoServer>();
    net::HostPortPair host_port_pair;
    ASSERT_TRUE(udp_echo_server_->Start(network_context, &host_port_pair));

    port_ = host_port_pair.port();
    ASSERT_GT(*port_, 0);
  }

  void Stop() override { udp_echo_server_.reset(); }

  uint16_t port() const override {
    DCHECK(port_);
    return *port_;
  }

 private:
  std::unique_ptr<extensions::TestUdpEchoServer> udp_echo_server_;
  std::optional<uint16_t> port_;
};

template <typename TestHarness>
  requires(std::is_base_of_v<InProcessBrowserTest, TestHarness>)
class ChromeDirectSocketsTest : public TestHarness {
 public:
  ChromeDirectSocketsTest() = delete;

  void SetUpOnMainThread() override {
    TestHarness::SetUpOnMainThread();
    TestHarness::host_resolver()->AddRule(kHostname, "127.0.0.1");
    test_server()->Start(InProcessBrowserTest::browser()
                             ->GetProfile()
                             ->GetDefaultStoragePartition()
                             ->GetNetworkContext());
  }

  void TearDownOnMainThread() override {
    TestHarness::TearDownOnMainThread();
    test_server()->Stop();
  }

 protected:
  explicit ChromeDirectSocketsTest(std::unique_ptr<TestServer> test_server)
      : test_server_{std::move(test_server)} {}
  TestServer* test_server() const {
    DCHECK(test_server_);
    return test_server_.get();
  }

 private:
  std::unique_ptr<TestServer> test_server_;
};

template <typename TestHarness>
class ChromeDirectSocketsTcpTest : public ChromeDirectSocketsTest<TestHarness> {
 public:
  ChromeDirectSocketsTcpTest()
      : ChromeDirectSocketsTest<TestHarness>{
            std::make_unique<TcpHttpTestServer>()} {}
};

template <typename TestHarness>
class ChromeDirectSocketsUdpTest : public ChromeDirectSocketsTest<TestHarness> {
 public:
  ChromeDirectSocketsUdpTest()
      : ChromeDirectSocketsTest<TestHarness>{
            std::make_unique<UdpEchoTestServer>()} {}
};

#if BUILDFLAG(ENABLE_EXTENSIONS)

class ChromeAppApiTest : public extensions::ExtensionApiTest {
 public:
  static constexpr std::string_view kWorkerScriptTemplate = R"(
    self.onmessage = async e => {
      try {
        await %s;
        self.postMessage(null);
      } catch (err) {
        self.postMessage({ error: err });
      }
    };
  )";

  static constexpr std::string_view kWorkerConnect = R"(
    new Promise((resolve, reject) => {
      const policy = trustedTypes.createPolicy("default", {
        createScriptURL: (url) => url,
      });
      const worker = new Worker(
        policy.createScriptURL('/worker.js')
      );
      worker.onmessage = e => {
        if (e.data) {
          reject(e.data.error);
        } else {
          resolve();
        }
      };
      worker.postMessage(null);
    });
  )";
  content::RenderFrameHost* InstallAndOpenChromeApp(
      const base::DictValue& manifest) {
    dir_.WriteManifest(manifest);
    dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    return InstallAndOpenChromeApp();
  }

  content::RenderFrameHost* InstallAndOpenChromeAppWithWorkerScript(
      const base::DictValue& manifest,
      std::string_view worker_script) {
    dir_.WriteManifest(manifest);
    dir_.WriteFile(FILE_PATH_LITERAL("background.js"), "");
    dir_.WriteFile(FILE_PATH_LITERAL("worker.js"), worker_script);
    return InstallAndOpenChromeApp();
  }

 private:
  content::RenderFrameHost* InstallAndOpenChromeApp() {
    const extensions::Extension& extension =
        CHECK_DEREF(LoadExtension(dir_.UnpackedPath()));
    return CHECK_DEREF(extensions::ProcessManager::Get(profile())
                           ->GetBackgroundHostForExtension(extension.id()))
        .main_frame_host();
  }

  extensions::TestExtensionDir dir_;
};

using ChromeDirectSocketsTcpApiTest =
    ChromeDirectSocketsTcpTest<ChromeAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest, TcpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/
                       base::DictValue().Set(
                           "tcp", base::DictValue().Set("connect", "*"))));

  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile());

  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::LOCAL_NETWORK,
      ContentSetting::CONTENT_SETTING_ALLOW);
  host_content_settings_map->SetDefaultContentSetting(
      ContentSettingsType::LOOPBACK_NETWORK,
      ContentSetting::CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(content::ExecJs(app_frame,
                              content::JsReplace(kTcpReadWriteScript, kHostname,
                                                 test_server()->port())));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest, TcpReadWriteFromWorker) {
  const std::string worker_script = base::StringPrintf(
      kWorkerScriptTemplate, content::JsReplace(kTcpReadWriteScript, kHostname,
                                                test_server()->port()));

  content::RenderFrameHost* app_frame = InstallAndOpenChromeAppWithWorkerScript(
      GenerateManifest(/*socket_permissions=*/base::DictValue().Set(
          "tcp", base::DictValue().Set("connect", "*"))),
      worker_script);

  ASSERT_TRUE(content::ExecJs(app_frame, kWorkerConnect));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest,
                       TcpSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      return typeof TCPSocket === 'undefined';
    })();
  )";

  EXPECT_EQ(true, EvalJs(app_frame, kScript));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpApiTest,
                       TcpFailsWithoutSocketsTcpConnectPermission) {
  // "sockets" key is present in the manifest, but "sockets.tcp.connect" is not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPSocket($1, $2);
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname,
                                                   test_server()->port())),
              ErrorIs(AccessBlocked()));
}

using ChromeDirectSocketsUdpApiTest =
    ChromeDirectSocketsUdpTest<ChromeAppApiTest>;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue().Set(
          "udp", base::DictValue().Set("send", "*"))));

  ASSERT_TRUE(content::ExecJs(
      app_frame, content::JsReplace(kUdpConnectedReadWriteScript, kHostname,
                                    test_server()->port())));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpReadWriteFromWorker) {
  const std::string worker_script =
      base::StringPrintf(kWorkerScriptTemplate,
                         content::JsReplace(kUdpConnectedReadWriteScript,
                                            kHostname, test_server()->port()));

  content::RenderFrameHost* app_frame = InstallAndOpenChromeAppWithWorkerScript(
      GenerateManifest(/*socket_permissions=*/base::DictValue().Set(
          "udp", base::DictValue().Set("send", "*"))),
      worker_script);

  ASSERT_TRUE(content::ExecJs(app_frame, kWorkerConnect));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      return typeof UDPSocket === 'undefined';
    })();
  )";

  EXPECT_EQ(true, EvalJs(app_frame, kScript));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpConnectedFailsWithoutSocketsUdpSendPermission) {
  // "sockets" key is present in the manifest, but "sockets.udp.send" is
  // not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new UDPSocket({ remoteAddress: $1, remotePort: $2 });
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, content::JsReplace(kScript, kHostname,
                                                   test_server()->port())),
              ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest,
                       UdpBoundFailsWithoutSocketsUdpBindPermission) {
  // "sockets" key is present in the manifest as well as "sockets.udp.send",
  // but "sockets.udp.bind" is not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "::" });
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, kScript), ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsUdpApiTest, UdpServerReadWrite) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue().Set(
          "udp", base::DictValue().Set("bind", "*").Set("send", "*"))));

  ASSERT_TRUE(content::ExecJs(
      app_frame, content::JsReplace(kUdpBoundReadWriteScript, kHostname,
                                    test_server()->port())));
}

IN_PROC_BROWSER_TEST_F(
    ChromeDirectSocketsUdpApiTest,
    UdpServerNotAffectedByLocalNetworkAccessContentSettingInChromeApps) {
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue().Set(
          "udp", base::DictValue().Set("bind", "*").Set("send", "*"))));

  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::LOCAL_NETWORK,
                                 ContentSetting::CONTENT_SETTING_BLOCK);
  HostContentSettingsMapFactory::GetForProfile(profile())
      ->SetDefaultContentSetting(ContentSettingsType::LOOPBACK_NETWORK,
                                 ContentSetting::CONTENT_SETTING_BLOCK);

  constexpr std::string_view kUdpBoundPna = R"(
    (async () => {
      const socket = new UDPSocket({ localAddress: "0.0.0.0" });
      await socket.opened;
    })();
  )";

  ASSERT_TRUE(content::ExecJs(app_frame, kUdpBoundPna));
}

using ChromeDirectSocketsTcpServerApiTest = ChromeAppApiTest;

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerSocketUndefinedWithoutSocketsPermission) {
  // "sockets" key is not present in the manifest.
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest());

  static constexpr std::string_view kScript = R"(
    (async () => {
      return typeof TCPServerSocket === 'undefined';
    })();
  )";

  EXPECT_EQ(true, EvalJs(app_frame, kScript));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerFailsWithoutSocketsTcpServerListenPermission) {
  // "sockets" key is present in the manifest, but "sockets.tcpServer.listen" is
  // not.
  content::RenderFrameHost* app_frame = InstallAndOpenChromeApp(
      GenerateManifest(/*socket_permissions=*/base::DictValue()));

  static constexpr std::string_view kScript = R"(
    (async () => {
      const socket = new TCPServerSocket("::");
      await socket.opened;
    })();
  )";

  EXPECT_THAT(EvalJs(app_frame, kScript), ErrorIs(AccessBlocked()));
}

IN_PROC_BROWSER_TEST_F(ChromeDirectSocketsTcpServerApiTest,
                       TcpServerExchangePacketWithTcpSocket) {
  content::RenderFrameHost* app_frame =
      InstallAndOpenChromeApp(GenerateManifest(
          /*socket_permissions=*/base::DictValue()
              .Set("tcpServer", base::DictValue().Set("listen", "*"))
              .Set("tcp", base::DictValue().Set("connect", "*"))));

  ASSERT_TRUE(
      content::ExecJs(app_frame, kTcpServerExchangePacketWithTcpScript));
}

#endif

}  // namespace
