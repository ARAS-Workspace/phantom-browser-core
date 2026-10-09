// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

constexpr char kWorkerJS[] = R"(
  function verifyData(data) {
    if (data.byteLength != 16)
      return `Improper byteLength: ${data.byteLength}`;

    const bufView = new Uint8Array(data);
    for (let i = 0; i < 16; i++) {
      if (bufView[i] != i % 2) {
        return `Data mismatch at index ${i}: Expected: ${i % 2}, got: ${
            bufView[i]}`;
      }
    }

    return 'PASS';
  }

  self.addEventListener('message', e => {
    try {
      postMessage(verifyData(e.data));
    } catch (e) {
      postMessage(e.message);
    }
  });
)";

constexpr char kBackgroundJS_SabAllowed[] = R"(
  chrome.test.runTests([
    function sendSharedArrayBufferToWorker() {
      let sab = new SharedArrayBuffer(16);
      let bufView = new Uint8Array(sab);
      for (let i = 0; i < 16; i++)
        bufView[i] = (i % 2);

      const workerUrl = chrome.runtime.getURL('worker.js');
      let worker = new Worker(workerUrl);

      worker.onmessage = e => {
        chrome.test.assertEq('PASS', e.data);
        chrome.test.succeed();
      };

      worker.postMessage(sab);
      chrome.test.assertEq(16, sab.byteLength);

      // The worker will ack on receiving the SharedArrayBuffer causing the test
      // to terminate.
    }
  ]);
)";

// Parameterized on is_cross_origin_isolated.
class SharedArrayBufferTest
    : public ExtensionApiTest,
      public ::testing::WithParamInterface<bool> {
 public:

  TestExtensionDir& test_dir() { return test_dir_; }

 private:
  TestExtensionDir test_dir_;
};

IN_PROC_BROWSER_TEST_P(SharedArrayBufferTest, TransferToWorker) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const bool is_cross_origin_isolated = GetParam();

  auto builder = base::DictValue()
                     .Set("manifest_version", 2)
                     .Set("name", "SharedArrayBuffer")
                     .Set("version", "1.1");

  if (is_cross_origin_isolated) {
    builder.Set("cross_origin_opener_policy",
                base::DictValue().Set("value", "same-origin"));
    builder.Set("cross_origin_embedder_policy",
                base::DictValue().Set("value", "require-corp"));
  }

  base::DictValue background_builder;
  background_builder.Set("scripts", base::ListValue().Append("background.js"));

  builder.Set("background", std::move(background_builder));

  test_dir().WriteManifest(builder);

  test_dir().WriteFile(FILE_PATH_LITERAL("background.js"),
                       kBackgroundJS_SabAllowed);
  test_dir().WriteFile(FILE_PATH_LITERAL("worker.js"), kWorkerJS);

  ASSERT_TRUE(RunExtensionTest(test_dir().Pack(), {}, {} /* load_options */))
      << message_;
}

INSTANTIATE_TEST_SUITE_P(
    ,
    SharedArrayBufferTest,
    ::testing::Bool(),
    [](const testing::TestParamInfo<bool>& info) {
      return base::StringPrintf("%s_Extension",
                                info.param ? "COI" : "NonCOI");
    });

}  // namespace extensions
