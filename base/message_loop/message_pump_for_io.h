// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_H_
#define BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_H_

// This header is a forwarding header to coalesce the various platform specific
// types representing MessagePumpForIO.

#include "base/message_loop/ios_cronet_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/message_loop/message_pump_kqueue.h"
#elif BUILDFLAG(IS_POSIX)
#include "base/message_loop/message_pump_epoll.h"
#endif

namespace base {

#if BUILDFLAG(IS_APPLE)
using MessagePumpForIO = MessagePumpKqueue;
#elif BUILDFLAG(IS_POSIX)
using MessagePumpForIO = MessagePumpEpoll;
#else
#error Platform does not define MessagePumpForIO
#endif

}  // namespace base

#endif  // BASE_MESSAGE_LOOP_MESSAGE_PUMP_FOR_IO_H_
