// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"

// static
void InstallationErrorInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    const extensions::CrxInstallError& error) {
  infobar_manager->AddInfoBar(
      CreateConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate>(
          new InstallationErrorInfoBarDelegate(error))));
}

InstallationErrorInfoBarDelegate::InstallationErrorInfoBarDelegate(
    const extensions::CrxInstallError& error)
    : error_(error) {}

InstallationErrorInfoBarDelegate::~InstallationErrorInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
InstallationErrorInfoBarDelegate::GetIdentifier() const {
  return INSTALLATION_ERROR_INFOBAR_DELEGATE;
}

std::u16string InstallationErrorInfoBarDelegate::GetMessageText() const {
  return error_.message();
}

int InstallationErrorInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}
