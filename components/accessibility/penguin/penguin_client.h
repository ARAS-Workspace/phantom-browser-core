// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_
#define COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace gfx {
class Rect;
}  // namespace gfx

namespace penguin {

// Use this callback to get only the main text from the API call.
using PenguinCompactResponseCallback =
    base::OnceCallback<void(const std::string&)>;

// Use this callback format to get all information from the API call, which
// will include response codes and all parts of the response (possibly
// multiple candidates, finish reason, safety ratings, etc.).
using PenguinFullResponseCallback =
    base::OnceCallback<void(int32_t response_code,
                            int32_t net_error_code,
                            const std::string& main_content)>;

class PenguinClient {
 public:
  static PenguinClient* Create(content::BrowserContext* context);

  explicit PenguinClient(content::BrowserContext* context);

  ~PenguinClient();
  PenguinClient(const PenguinClient& client) = delete;
  PenguinClient& operator=(const PenguinClient& client) = delete;

  void PerformAPICall(const std::string& image_data,
                      const std::string& text_input,
                      PenguinCompactResponseCallback callback);
  void PerformAPICall(const std::string& image_data,
                      const std::string& text_input,
                      PenguinFullResponseCallback callback);

  void PerformAPICall(content::WebContents* web_contents,
                      const std::string& text_input,
                      PenguinCompactResponseCallback callback);
  void PerformAPICall(content::WebContents* web_contents,
                      const std::string& text_input,
                      PenguinFullResponseCallback callback);

  void PerformAPICall(content::WebContents* web_contents,
                      const std::string& text_input,
                      PenguinCompactResponseCallback callback,
                      const gfx::Rect& source_rect);
  void PerformAPICall(content::WebContents* web_contents,
                      const std::string& text_input,
                      PenguinFullResponseCallback callback,
                      const gfx::Rect& source_rect);

  void PerformAPICall(const std::string& text_input,
                      PenguinCompactResponseCallback callback);
  void PerformAPICall(const std::string& text_input,
                      PenguinFullResponseCallback callback);
};

}  // namespace penguin

#endif  // COMPONENTS_ACCESSIBILITY_PENGUIN_PENGUIN_CLIENT_H_
