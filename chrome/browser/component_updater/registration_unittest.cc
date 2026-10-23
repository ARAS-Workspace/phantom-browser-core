// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/registration.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

class RegistrationTest : public testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::ScopedPathOverride user_data_dir_override_{chrome::DIR_USER_DATA};
};

TEST_F(RegistrationTest, RegistersOnlyCrlSetAndPkiMetadata) {
  auto service =
      std::make_unique<testing::NiceMock<MockComponentUpdateService>>();
  std::vector<std::string> names;
  EXPECT_CALL(*service, RegisterComponent)
      .WillRepeatedly([&names](const ComponentRegistration& component) {
        names.push_back(component.name);
        return true;
      });
  TestingBrowserProcess::GetGlobal()->SetComponentUpdater(std::move(service));

  RegisterComponentsForUpdate();
  task_environment_.RunUntilIdle();

  EXPECT_THAT(names, testing::UnorderedElementsAre("CRLSet", "PKI Metadata"));

  TestingBrowserProcess::GetGlobal()->SetComponentUpdater(nullptr);
}

}  // namespace component_updater
