// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/crash_ids_source.h"

#include "build/build_config.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/upload_list/upload_list.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class StubUploadList : public UploadList {
 public:
  StubUploadList() = default;
  StubUploadList(const StubUploadList&) = delete;
  StubUploadList& operator=(const StubUploadList&) = delete;

 protected:
  ~StubUploadList() override = default;
  std::vector<std::unique_ptr<UploadInfo>> LoadUploadList() override {
    return {};
  }

  void ClearUploadList(const base::Time& begin,
                       const base::Time& end) override {}

  void RequestSingleUpload(const std::string& local_id) override {}
};

}  // namespace system_logs
