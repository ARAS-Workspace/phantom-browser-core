// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/intent_util.h"

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chromeos/ash/experiences/arc/intent_helper/intent_constants.h"
#include "chromeos/ash/experiences/arc/intent_helper/intent_filter.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using apps::Condition;
using apps::ConditionType;
using apps::IntentFilterPtr;
using apps::IntentFilters;
using apps::PatternMatchType;

class IntentUtilsTest : public testing::Test {
 protected:
};

TEST_F(IntentUtilsTest, CreateNoteTakingFilter) {
  IntentFilterPtr filter = apps_util::CreateNoteTakingFilter();

  ASSERT_EQ(filter->conditions.size(), 1u);
  const Condition& condition = *filter->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionCreateNote);

  EXPECT_TRUE(apps_util::CreateCreateNoteIntent()->MatchFilter(filter));
}

TEST_F(IntentUtilsTest, CreateLockScreenFilter) {
  IntentFilterPtr filter = apps_util::CreateLockScreenFilter();

  ASSERT_EQ(filter->conditions.size(), 1u);
  const Condition& condition = *filter->conditions[0];
  EXPECT_EQ(condition.condition_type, ConditionType::kAction);
  ASSERT_EQ(condition.condition_values.size(), 1u);
  EXPECT_EQ(condition.condition_values[0]->value,
            apps_util::kIntentActionStartOnLockScreen);

  EXPECT_TRUE(apps_util::CreateStartOnLockScreenIntent()->MatchFilter(filter));
}

TEST_F(IntentUtilsTest, CreateIntentFiltersForChromeApp_FileHandlers) {
  // Foo app provides file handler for text/plain and all file types.
  extensions::ExtensionBuilder foo_app("Foo");
  static constexpr char kManifest[] = R"(
    "manifest_version": 2,
    "version": "1.0.0",
    "app": {
      "background": {
        "scripts": ["background.js"]
      }
    },
    "file_handlers": {
      "any": {
        "types": ["*/*"]
      },
      "text": {
        "extensions": ["txt"],
        "types": ["text/plain"],
        "verb": "open_with"
      }
    }
  )";
  foo_app.AddJSON(kManifest).BuildManifest();
  scoped_refptr<const extensions::Extension> foo = foo_app.Build();

  IntentFilters filters = apps_util::CreateIntentFiltersForChromeApp(foo.get());

  ASSERT_EQ(filters.size(), 2u);

  // "any" filter - View action
  const IntentFilterPtr& mime_filter = filters[0];
  ASSERT_EQ(mime_filter->conditions.size(), 2u);
  const Condition& view_cond = *mime_filter->conditions[0];
  EXPECT_EQ(view_cond.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond.condition_values.size(), 1u);
  EXPECT_EQ(view_cond.condition_values[0]->value, apps_util::kIntentActionView);

  // "any" filter - mime type match
  const Condition& file_cond = *mime_filter->conditions[1];
  EXPECT_EQ(file_cond.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond.condition_values.size(), 1u);
  EXPECT_EQ(file_cond.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond.condition_values[0]->value, "*/*");

  // Text filter - View action
  const IntentFilterPtr& mime_filter2 = filters[1];
  ASSERT_EQ(mime_filter2->conditions.size(), 2u);
  const Condition& view_cond2 = *mime_filter2->conditions[0];
  EXPECT_EQ(view_cond2.condition_type, ConditionType::kAction);
  ASSERT_EQ(view_cond2.condition_values.size(), 1u);
  EXPECT_EQ(view_cond2.condition_values[0]->value,
            apps_util::kIntentActionView);

  // Text filter - mime type match
  const Condition& file_cond2 = *mime_filter2->conditions[1];
  EXPECT_EQ(file_cond2.condition_type, ConditionType::kFile);
  ASSERT_EQ(file_cond2.condition_values.size(), 2u);
  EXPECT_EQ(file_cond2.condition_values[0]->match_type,
            PatternMatchType::kMimeType);
  EXPECT_EQ(file_cond2.condition_values[0]->value, "text/plain");
  // Text filter - file extension match
  EXPECT_EQ(file_cond2.condition_values[1]->match_type,
            PatternMatchType::kFileExtension);
  EXPECT_EQ(file_cond2.condition_values[1]->value, "txt");
}
