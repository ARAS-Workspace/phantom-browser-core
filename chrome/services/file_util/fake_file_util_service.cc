// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/fake_file_util_service.h"

#include "build/build_config.h"

FakeFileUtilService::FakeFileUtilService(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeFileUtilService::~FakeFileUtilService() = default;

#if BUILDFLAG(ENABLE_EXTRACTORS)
void FakeFileUtilService::BindSingleFileTarXzFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED();
}

void FakeFileUtilService::BindSingleFileTarFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED();
}
#endif

