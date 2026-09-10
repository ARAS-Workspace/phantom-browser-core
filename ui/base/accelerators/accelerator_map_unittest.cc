// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/accelerator_map.h"

#include <utility>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

namespace {

bool IsValidMatch(AcceleratorMap<int>* map,
                  const Accelerator& pressed,
                  int expected) {
  return nullptr != map->Find(pressed) && expected == *map->Find(pressed) &&
         expected == map->Get(pressed);
}

TEST(AcceleratorMapTest, MapIsEmpty) {
  AcceleratorMap<int> m;
  EXPECT_EQ(0U, m.size());
  EXPECT_TRUE(m.empty());
}

TEST(AcceleratorMapTest, EmptyFind) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  EXPECT_EQ(nullptr, m.Find(accelerator));

  // Still empty after lookup.
  EXPECT_TRUE(m.empty());
}

TEST(AcceleratorMapTest, FindExists) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_TRUE(IsValidMatch(&m, accelerator, expected));

  // Still single entry.
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, FindDoesNotExist) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  Accelerator other(VKEY_Y, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_EQ(nullptr, m.Find(other));

  // Still single entry.
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, InsertDefaultCreatedNew) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  int& value_ref = m.GetOrInsertDefault(accelerator);
  EXPECT_EQ(int(), value_ref);
  EXPECT_EQ(1U, m.size());
}

TEST(AcceleratorMapTest, ChangeValueViaReturnedRef) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  int& value_ref = m.GetOrInsertDefault(accelerator);

  const int expected = 77;
  value_ref = expected;
  EXPECT_EQ(expected, m.Get(accelerator));
}

TEST(AcceleratorMapTest, SetValueDirect) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);

  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));
  EXPECT_EQ(expected, m.Get(accelerator));
}

TEST(AcceleratorMapTest, Iterate) {
  AcceleratorMap<int> m;
  Accelerator accelerator(VKEY_Z, EF_SHIFT_DOWN);
  const int expected = 77;
  m.InsertNew(std::make_pair(accelerator, expected));

  auto iter = m.begin();
  EXPECT_NE(m.end(), iter);
  EXPECT_EQ(accelerator, iter->first);
  EXPECT_EQ(expected, iter->second);
  ++iter;
  EXPECT_EQ(m.end(), iter);
}

}  // namespace

}  // namespace ui
