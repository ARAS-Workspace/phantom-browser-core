// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/fqdn.h"

#include "build/build_config.h"

#include "net/base/network_interfaces.h"

namespace remoting {

std::string GetFqdn() {
  return net::GetHostName();
}

}  // namespace remoting
