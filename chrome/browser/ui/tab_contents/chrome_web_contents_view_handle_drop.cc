// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.h"

#include <algorithm>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/glic/host/glic_drag_and_drop_util.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/enterprise/common/files_scan_data.h"
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/clipboard/file_info.h"

void HandleOnPerformingDrop(
    content::WebContents* web_contents,
    content::DropData drop_data,
    content::WebContentsViewDelegate::DropCompletionCallback callback) {
  CHECK(callback);

  content::WebContents* scan_target =
      glic::GetGlicGuestWebContents(web_contents);
  if (scan_target) {
    // For Glic-specific web sourced drag and drop operations, we bypass the
    // standard Enterprise Cloud Content Analysis scanning (Safe Browsing/deep
    // scanning) here. This is intended because we will run DLP scanning for
    // the payload as part of the Glic invocation, and if that fails the user
    // won't see the content anyway.
    if (glic::IsGlicWebDrag(drop_data)) {
      glic::StartDragAndDropInvoke(web_contents, drop_data);
      drop_data.document_is_handling_drag = true;
      std::move(callback).Run(std::move(drop_data));
      return;
    }
  } else {
    scan_target = web_contents;
  }

  absl::Cleanup cleanup = [&] {
    if (scan_target->GetDelegate()) {
      scan_target->GetDelegate()->HandleDragEnded();
    }
    std::move(callback).Run(std::move(drop_data));
  };

  // If content analysis is not available, make sure that the renderer never
  // forces a default action.
  drop_data.document_is_handling_drag = true;
  return;
}
