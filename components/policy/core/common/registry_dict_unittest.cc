// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_dict.h"

#include <memory>
#include <string>
#include <utility>

#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/schema.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

TEST(RegistryDictTest, SetAndGetValue) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));

  test_dict.SetValue("two", string_value.Clone());
  EXPECT_EQ(2u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  std::optional<base::Value> one(test_dict.RemoveValue("one"));
  ASSERT_TRUE(one.has_value());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, one.value());
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_EQ(string_value, *test_dict.GetValue("two"));

  test_dict.ClearValues();
  EXPECT_FALSE(test_dict.GetValue("one"));
  EXPECT_FALSE(test_dict.GetValue("two"));
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingValueNames) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("One", int_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(int_value, *test_dict.GetValue("oNe"));

  auto entry = test_dict.values().begin();
  ASSERT_NE(entry, test_dict.values().end());
  EXPECT_EQ("One", entry->first);

  test_dict.SetValue("ONE", string_value.Clone());
  EXPECT_EQ(1u, test_dict.values().size());
  EXPECT_EQ(string_value, *test_dict.GetValue("one"));

  std::optional<base::Value> removed_value(test_dict.RemoveValue("onE"));
  ASSERT_TRUE(removed_value.has_value());
  EXPECT_EQ(string_value, removed_value.value());
  EXPECT_TRUE(test_dict.values().empty());
}

TEST(RegistryDictTest, SetAndGetKeys) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("one", int_value.Clone());
  test_dict.SetKey("two", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));

  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("three", string_value.Clone());
  test_dict.SetKey("four", std::move(subdict));
  EXPECT_EQ(2u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("two");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("one"));
  actual_subdict = test_dict.GetKey("four");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("three"));

  test_dict.ClearKeys();
  EXPECT_FALSE(test_dict.GetKey("one"));
  EXPECT_FALSE(test_dict.GetKey("three"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, CaseInsensitiveButPreservingKeyNames) {
  RegistryDict test_dict;

  base::Value int_value(42);

  test_dict.SetKey("One", std::make_unique<RegistryDict>());
  EXPECT_EQ(1u, test_dict.keys().size());
  RegistryDict* actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_TRUE(actual_subdict->values().empty());

  auto entry = test_dict.keys().begin();
  ASSERT_NE(entry, test_dict.keys().end());
  EXPECT_EQ("One", entry->first);

  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", int_value.Clone());
  test_dict.SetKey("ONE", std::move(subdict));
  EXPECT_EQ(1u, test_dict.keys().size());
  actual_subdict = test_dict.GetKey("One");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));

  std::unique_ptr<RegistryDict> removed_key(test_dict.RemoveKey("one"));
  ASSERT_TRUE(removed_key);
  EXPECT_EQ(int_value, *removed_key->GetValue("two"));
  EXPECT_TRUE(test_dict.keys().empty());
}

TEST(RegistryDictTest, Merge) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.Clone());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.Clone());
  dict_a.SetKey("three", std::move(subdict));

  dict_b.SetValue("four", string_value.Clone());
  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("two", int_value.Clone());
  dict_b.SetKey("three", std::move(subdict));
  subdict = std::make_unique<RegistryDict>();
  subdict->SetValue("five", int_value.Clone());
  dict_b.SetKey("six", std::move(subdict));

  dict_a.Merge(dict_b);

  EXPECT_EQ(int_value, *dict_a.GetValue("one"));
  EXPECT_EQ(string_value, *dict_b.GetValue("four"));
  RegistryDict* actual_subdict = dict_a.GetKey("three");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("two"));
  actual_subdict = dict_a.GetKey("six");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(int_value, *actual_subdict->GetValue("five"));
}

TEST(RegistryDictTest, Swap) {
  RegistryDict dict_a;
  RegistryDict dict_b;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  dict_a.SetValue("one", int_value.Clone());
  dict_a.SetKey("two", std::make_unique<RegistryDict>());
  dict_b.SetValue("three", string_value.Clone());

  dict_a.Swap(&dict_b);

  EXPECT_EQ(int_value, *dict_b.GetValue("one"));
  EXPECT_TRUE(dict_b.GetKey("two"));
  EXPECT_FALSE(dict_b.GetValue("two"));

  EXPECT_EQ(string_value, *dict_a.GetValue("three"));
  EXPECT_FALSE(dict_a.GetValue("one"));
  EXPECT_FALSE(dict_a.GetKey("two"));
}

TEST(RegistryDictTest, KeyValueNameClashes) {
  RegistryDict test_dict;

  base::Value int_value(42);
  base::Value string_value("fortytwo");

  test_dict.SetValue("one", int_value.Clone());
  std::unique_ptr<RegistryDict> subdict(new RegistryDict());
  subdict->SetValue("two", string_value.Clone());
  test_dict.SetKey("one", std::move(subdict));

  EXPECT_EQ(int_value, *test_dict.GetValue("one"));
  RegistryDict* actual_subdict = test_dict.GetKey("one");
  ASSERT_TRUE(actual_subdict);
  EXPECT_EQ(string_value, *actual_subdict->GetValue("two"));
}

}  // namespace
}  // namespace policy
