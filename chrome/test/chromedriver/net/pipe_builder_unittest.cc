// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"

#include "base/command_line.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/eintr_wrapper.h"
#endif

namespace {

#if BUILDFLAG(IS_POSIX)
testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}
#endif

class PipeBuilderTest : public testing::Test {
 protected:
  PipeBuilderTest() : long_timeout_(base::Minutes(1)) {}
  ~PipeBuilderTest() override = default;

  Timeout long_timeout() const { return Timeout(long_timeout_); }

  base::CommandLine CreateCommandLine() {
    return base::GetMultiProcessTestChildBaseCommandLine();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  const base::TimeDelta long_timeout_;
};

enum {
  kSuccess = 0,
  kReadError = 1,
  kWriteError = 2,
  kInvalidInPipe = 3,
  kInvalidOutPipe = 4,
  kIoPipesNotFound = 5,
  kIoPipesAreMalformed = 6,
};

#if BUILDFLAG(IS_POSIX)
int ReadFromPipeNoBestEffort(base::PlatformFile file_in,
                             char* buffer,
                             int size) {
  return HANDLE_EINTR(read(file_in, buffer, size));
}
#endif

#if BUILDFLAG(IS_POSIX)
int WriteToPipeNoBestEffort(base::PlatformFile file_out,
                            base::span<const char> buffer) {
  return HANDLE_EINTR(write(file_out, buffer.data(), buffer.size()));
}
#endif

#if BUILDFLAG(IS_POSIX)
int WriteToPipe(base::PlatformFile file_out, base::span<const char> buffer) {
  size_t offset = 0;
  int rv = 0;
  for (; offset < buffer.size(); offset += rv) {
    rv = WriteToPipeNoBestEffort(file_out, buffer.subspan(offset));
    if (rv < 0) {
      return rv;
    }
  }
  return static_cast<int>(offset);
}
#endif

MULTIPROCESS_TEST_MAIN(PipeEchoProcess) {
  const int capacity = 1024;
  base::ScopedPlatformFile file_in;
  base::ScopedPlatformFile file_out;
#if BUILDFLAG(IS_POSIX)
  file_in = base::ScopedPlatformFile(3);
  file_out = base::ScopedPlatformFile(4);
#endif
  std::vector<char> buffer(capacity);
  while (true) {
    int bytes_read =
        ReadFromPipeNoBestEffort(file_in.get(), buffer.data(), buffer.size());
    // read_bytes < 0 means an error
    // read_bytes == 0 means EOF
    if (bytes_read < 0) {
      return kReadError;
    }
    if (bytes_read == 0) {
      // EOF
      break;
    }
    int bytes_written =
        WriteToPipe(file_out.get(),
                    base::span(buffer).first(static_cast<size_t>(bytes_read)));
    if (bytes_written < 0) {
      return kWriteError;
    }
  }
  return kSuccess;
}

}  // namespace

TEST_F(PipeBuilderTest, Ctor) {
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, NoProtocolModeIsProvided) {
  PipeBuilder pipe_builder;
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options, &command).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, CborIsUnsupported) {
  PipeBuilder pipe_builder;
  EXPECT_STREQ("cbor", PipeBuilder::kCborProtocolMode);
  pipe_builder.SetProtocolMode(PipeBuilder::kCborProtocolMode);
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options, &command).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#if BUILDFLAG(IS_POSIX)

TEST_F(PipeBuilderTest, PlatfformIsSupported) {
  EXPECT_TRUE(PipeBuilder::PlatformIsSupported());
}

TEST_F(PipeBuilderTest, CloseChildEndpointsWhenNotStarted) {
  PipeBuilder pipe_builder;
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
}

TEST_F(PipeBuilderTest, EmptyStringProtocolMode) {
  PipeBuilder pipe_builder;
  pipe_builder.SetProtocolMode("");
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options, &command)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
  std::unique_ptr<SyncWebSocket> socket = pipe_builder.TakeSocket();
  EXPECT_NE(nullptr, socket.get());
}

TEST_F(PipeBuilderTest, SendAndReceive) {
  PipeBuilder pipe_builder;
  pipe_builder.SetProtocolMode(PipeBuilder::kAsciizProtocolMode);
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options, &command)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
#if BUILDFLAG(IS_POSIX)
  options.fds_to_remap.emplace_back(1, 1);
  options.fds_to_remap.emplace_back(2, 2);
#endif
  base::Process process =
      base::SpawnMultiProcessTestChild("PipeEchoProcess", command, options);
  ASSERT_TRUE(process.IsValid());
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
  std::unique_ptr<SyncWebSocket> socket = pipe_builder.TakeSocket();
  EXPECT_NE(nullptr, socket.get());
  EXPECT_TRUE(socket->Connect(GURL()));
  const std::string sent_message = "Hello, pipes!";
  EXPECT_TRUE(socket->Send(sent_message));
  EXPECT_TRUE(socket->IsConnected());
  std::string received_message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            socket->ReceiveNextMessage(&received_message, long_timeout()));
  EXPECT_TRUE(socket->IsConnected());
  EXPECT_EQ(sent_message, received_message);
  socket.reset();
  int exit_code = -1;
  process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

#else  // unsupported platforms

TEST_F(PipeBuilderTest, PlatformIsUnsupported) {
  EXPECT_FALSE(PipeBuilder::PlatformIsSupported());
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_TRUE(pipe_builder.CloseChildEndpoints().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#endif
