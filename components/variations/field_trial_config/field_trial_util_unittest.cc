// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_config/field_trial_util.h"

#include <map>
#include <string>

#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

class FieldTrialUtilTest : public ::testing::Test {
 public:
  FieldTrialUtilTest() = default;
  FieldTrialUtilTest(const FieldTrialUtilTest&) = delete;
  FieldTrialUtilTest& operator=(const FieldTrialUtilTest&) = delete;

  ~FieldTrialUtilTest() override {
    // Ensure that the maps are cleared between tests, since they are stored as
    // process singletons.
    test::ClearAllVariationIDs();
    test::ClearAllVariationParams();
  }
};

}  // namespace

TEST_F(FieldTrialUtilTest, AssociateParamsFromString) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.B:a/%2F";
  ASSERT_TRUE(AssociateParamsFromString(kVariationsString));

  base::FieldTrialList::CreateFieldTrial(kTrialName, "B");
  EXPECT_EQ("/", base::GetFieldTrialParamValue(kTrialName, "a"));
  EXPECT_EQ(std::string(), base::GetFieldTrialParamValue(kTrialName, "b"));
  EXPECT_EQ(std::string(), base::GetFieldTrialParamValue(kTrialName, "x"));

  std::map<std::string, std::string> params;
  EXPECT_TRUE(base::GetFieldTrialParams(kTrialName, &params));
  EXPECT_EQ(1U, params.size());
  EXPECT_EQ("/", params["a"]);
}

TEST_F(FieldTrialUtilTest, AssociateParamsFromStringWithSameTrial) {
  const std::string kTrialName = "AssociateVariationParams";
  const std::string kVariationsString =
      "AssociateVariationParams.A:a/10/b/test,AssociateVariationParams.A:a/x";
  ASSERT_FALSE(AssociateParamsFromString(kVariationsString));
}

TEST_F(FieldTrialUtilTest, TestEscapeValue) {
  std::string str = "trail.:/,*";
  std::string escaped_str = EscapeValue(str);
  EXPECT_EQ(escaped_str.find('.'), std::string::npos);
  EXPECT_EQ(escaped_str.find(':'), std::string::npos);
  EXPECT_EQ(escaped_str.find('/'), std::string::npos);
  EXPECT_EQ(escaped_str.find(','), std::string::npos);
  EXPECT_EQ(escaped_str.find('*'), std::string::npos);

  // Make sure the EscapeValue function is the inverse of base::UnescapeValue.
  EXPECT_EQ(str, base::UnescapeValue(escaped_str));
}

}  // namespace variations
