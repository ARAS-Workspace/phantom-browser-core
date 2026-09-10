// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/test/gtest_tags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/api/system_display/display_info_provider.h"
#include "extensions/browser/api/system_display/system_display_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/mock_display_info_provider.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/api/system_display.h"
#include "extensions/common/extension_builder.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

class SystemDisplayApiTest : public ExtensionApiTest {
 public:
  SystemDisplayApiTest()
      : provider_(std::make_unique<MockDisplayInfoProvider>()) {}

  SystemDisplayApiTest(const SystemDisplayApiTest&) = delete;
  SystemDisplayApiTest& operator=(const SystemDisplayApiTest&) = delete;

  ~SystemDisplayApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    DisplayInfoProvider::InitializeForTesting(provider_.get());
  }

 protected:
  void SetInfo(const std::string& display_id,
               const api::system_display::DisplayProperties& properties) {
    provider_->SetDisplayProperties(
        display_id, properties,
        base::BindOnce([](std::optional<std::string>) {}));
  }
  std::unique_ptr<MockDisplayInfoProvider> provider_;
};

}  // namespace extensions
