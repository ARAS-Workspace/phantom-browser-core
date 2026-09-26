// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/metadata_data_source.h"

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_trace_event.pbzero.h"

namespace tracing {

TEST(MetadataDataSourceTest, TraceCaptureDatetimeBundleFormatting) {
  protozero::HeapBuffered<perfetto::protos::pbzero::ChromeEventBundle> bundle;

  // Test time: 2016-08-18 22:28:10 UTC
  base::Time::Exploded exploded = {
      .year = 2016,
      .month = 8,
      .day_of_month = 18,
      .hour = 22,
      .minute = 28,
      .second = 10,
  };
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCExploded(exploded, &time));

  MetadataDataSource::RecordTraceCaptureDatetime(time, bundle.get());

  std::string serialized = bundle.SerializeAsString();
  perfetto::protos::pbzero::ChromeEventBundle::Decoder decoder(serialized);

  bool found = false;
  for (auto it = decoder.metadata(); it; ++it) {
    perfetto::protos::pbzero::ChromeMetadata::Decoder metadata(*it);
    if (metadata.name().ToStdString() == "trace-capture-datetime") {
      found = true;
      EXPECT_EQ(metadata.string_value().ToStdString(), "2016-8-18 22:28:10");
    }
  }
  EXPECT_TRUE(found);
}

}  // namespace tracing
