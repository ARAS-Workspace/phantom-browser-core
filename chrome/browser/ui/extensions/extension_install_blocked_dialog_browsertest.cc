// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/webstore_private/chrome_webstore_private_api_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

class ExtensionInstallBlockedDialogTest : public DialogBrowserTest {
 public:
  ExtensionInstallBlockedDialogTest() = default;
  ~ExtensionInstallBlockedDialogTest() override = default;

  void ShowUi(const std::string& name) override {
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder("extension_name").Build();
    // Show the dialog through the webstorePrivate delegate, the path used by
    // webstorePrivate.beginInstallWithManifest3.
    extensions::ChromeWebstorePrivateAPIDelegate delegate;
    delegate.ShowExtensionInstallBlockedDialog(
        browser()->tab_strip_model()->GetWebContentsAt(0), extension.get(),
        message_, CreateExtensionIcon(), base::DoNothing());
  }

  // Creates a big icon so that dialog will downscale it.
  gfx::ImageSkia CreateExtensionIcon() {
    SkBitmap icon;
    icon.allocN32Pixels(800, 800);
    icon.eraseColor(SK_ColorBLUE);
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  void set_message(const std::u16string& message) { message_ = message; }

 private:
  std::u16string message_;
};

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogTest,
                       InvokeUi_WithoutCustomMessage) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogTest,
                       InvokeUi_WithCustomMessage) {
  set_message(u"message");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ExtensionInstallBlockedDialogTest,
                       InvokeUi_WithLongCustomMessage) {
  set_message(u"long\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\nmessage");
  ShowAndVerifyUi();
}
