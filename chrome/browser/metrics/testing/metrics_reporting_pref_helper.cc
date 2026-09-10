// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/metrics_reporting_pref_helper.h"

#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"

namespace metrics {

base::FilePath SetUpUserDataDirectoryForTesting(bool is_enabled) {
  base::DictValue local_state_dict;
  local_state_dict.SetByDottedPath(metrics::prefs::kMetricsReportingEnabled,
                                   is_enabled);

  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return base::FilePath();

  base::FilePath local_state_path =
      user_data_dir.Append(chrome::kLocalStateFilename);
  if (!JSONFileValueSerializer(local_state_path).Serialize(local_state_dict))
    return base::FilePath();
  return local_state_path;
}

}  // namespace metrics
