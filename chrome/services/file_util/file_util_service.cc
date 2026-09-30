// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/file_util_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/services/file_util/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(ENABLE_EXTRACTORS)
#include "chrome/services/file_util/single_file_tar_file_extractor.h"
#include "chrome/services/file_util/single_file_tar_xz_file_extractor.h"
#endif

FileUtilService::FileUtilService(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver)
    : receiver_(this, std::move(receiver)) {}

FileUtilService::~FileUtilService() = default;

#if BUILDFLAG(ENABLE_EXTRACTORS)
void FileUtilService::BindSingleFileTarFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SingleFileTarFileExtractor>(),
                              std::move(receiver));
}
void FileUtilService::BindSingleFileTarXzFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SingleFileTarXzFileExtractor>(),
                              std::move(receiver));
}
#endif
