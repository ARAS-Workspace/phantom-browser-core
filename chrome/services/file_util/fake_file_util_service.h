// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_

#include <optional>

#include "base/files/file.h"
#include "build/build_config.h"
#include "chrome/services/file_util/buildflags.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

// An implementation of chrome::mojom::FileUtilService that binds and exposes
// mock interfaces, for use in tests.
class FakeFileUtilService : public chrome::mojom::FileUtilService {
 public:
  explicit FakeFileUtilService(
      mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver);

  FakeFileUtilService(const FakeFileUtilService&) = delete;
  FakeFileUtilService& operator=(const FakeFileUtilService&) = delete;

  ~FakeFileUtilService() override;

 private:
  // chrome::mojom::FileUtilService implementation

#if BUILDFLAG(ENABLE_EXTRACTORS)
  void BindSingleFileTarXzFileExtractor(
      mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver)
      override;
  void BindSingleFileTarFileExtractor(
      mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver)
      override;
#endif

  mojo::Receiver<chrome::mojom::FileUtilService> receiver_;

};

#endif  // CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_
