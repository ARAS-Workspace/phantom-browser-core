// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility/penguin/penguin_client.h"

#include <memory>

#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace penguin {

// static
PenguinClient* PenguinClient::Create(content::BrowserContext* context) {
  return nullptr;
}

PenguinClient::PenguinClient(content::BrowserContext* context) {}

PenguinClient::~PenguinClient() = default;

void PenguinClient::PerformAPICall(const std::string& image_data,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(const std::string& image_data,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinCompactResponseCallback callback,
                                   const gfx::Rect& source_rect) {}

void PenguinClient::PerformAPICall(content::WebContents* web_contents,
                                   const std::string& text_input,
                                   PenguinFullResponseCallback callback,
                                   const gfx::Rect& source_rect) {}

void PenguinClient::PerformAPICall(const std::string& text_input,
                                   PenguinCompactResponseCallback callback) {}

void PenguinClient::PerformAPICall(const std::string& text_input,
                                   PenguinFullResponseCallback callback) {}

}  // namespace penguin
