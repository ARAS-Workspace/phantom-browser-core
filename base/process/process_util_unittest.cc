// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <string_view>
#include <tuple>

#include "base/check.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "base/debug/stack_trace.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <malloc.h>
#include <sched.h>
#include <sys/syscall.h>
#endif
#if BUILDFLAG(IS_POSIX)
#include <sys/resource.h>
#endif
#if BUILDFLAG(IS_POSIX)
#include <dlfcn.h>
#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#if BUILDFLAG(IS_POSIX)
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#endif
#if BUILDFLAG(IS_APPLE)
#include <mach/vm_param.h>
#include <malloc/malloc.h>
#endif
#if BUILDFLAG(IS_ANDROID)
#include "third_party/lss/linux_syscall_support.h"
#endif

#if BUILDFLAG(IS_MAC)
#include <mach/mach.h>

#include "base/apple/mach_port_rendezvous_mac.h"
#include "base/apple/scoped_mach_port.h"
#include "base/mac/process_requirement.h"
#endif

namespace base {

namespace {

const char kSignalFileSlow[] = "SlowChildProcess.die";
const char kSignalFileKill[] = "KilledChildProcess.die";
const char kTestHelper[] = "test_child_process";

#if BUILDFLAG(IS_POSIX)
const char kSignalFileTerm[] = "TerminatedChildProcess.die";
#endif

#if BUILDFLAG(IS_POSIX)
const int kExpectedStillRunningExitCode = 0;
#endif

// Sleeps until file filename is created.
void WaitToDie(const char* filename) {
  FILE* fp;
  do {
    PlatformThread::Sleep(Milliseconds(10));
    fp = fopen(filename, "r");
  } while (!fp);
  fclose(fp);
}

// Signals children they should die now.
void SignalChildren(const char* filename) {
  FILE* fp = fopen(filename, "w");
  fclose(fp);
}

// Using a pipe to the child to wait for an event was considered, but
// there were cases in the past where pipes caused problems (other
// libraries closing the fds, child deadlocking). This is a simple
// case, so it's not worth the risk.  Using wait loops is discouraged
// in most instances.
TerminationStatus WaitForChildTermination(ProcessHandle handle,
                                          int* exit_code) {
  // Now we wait until the result is something other than STILL_RUNNING.
  TerminationStatus status = TERMINATION_STATUS_STILL_RUNNING;
  const TimeDelta kInterval = Milliseconds(20);
  TimeDelta waited;
  do {
    status = GetTerminationStatus(handle, exit_code);
    PlatformThread::Sleep(kInterval);
    waited += kInterval;
  } while (status == TERMINATION_STATUS_STILL_RUNNING &&
           waited < TestTimeouts::action_max_timeout());

  return status;
}

}  // namespace

const int kSuccess = 0;

class ProcessUtilTest : public MultiProcessTest {
 public:
  void SetUp() override {
    ASSERT_TRUE(PathService::Get(DIR_OUT_TEST_DATA_ROOT, &test_helper_path_));
    test_helper_path_ = test_helper_path_.AppendASCII(kTestHelper);
  }

#if BUILDFLAG(IS_POSIX)
  // Spawn a child process that counts how many file descriptors are open.
  int CountOpenFDsInChild();
#endif
  // Converts the filename to a platform specific filepath.
  // On Android files can not be created in arbitrary directories.
  static std::string GetSignalFilePath(const char* filename);

 protected:
  FilePath test_helper_path_;
};

std::string ProcessUtilTest::GetSignalFilePath(const char* filename) {
#if BUILDFLAG(IS_ANDROID)
  FilePath tmp_dir;
  PathService::Get(DIR_TEMP, &tmp_dir);
  // Ensure the directory exists to avoid harder to debug issues later.
  CHECK(PathExists(tmp_dir));
  tmp_dir = tmp_dir.Append(filename);
  return tmp_dir.value();
#else
  return filename;
#endif
}

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return kSuccess;
}

// TODO(viettrungluu): This should be in a "MultiProcessTestTest".
TEST_F(ProcessUtilTest, SpawnChild) {
  Process process = SpawnChild("SimpleChildProcess");
  ASSERT_TRUE(process.IsValid());
  int exit_code;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileSlow).c_str());
  return kSuccess;
}

TEST_F(ProcessUtilTest, KillSlowChild) {
  const std::string signal_file = GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  Process process = SpawnChild("SlowChildProcess");
  ASSERT_TRUE(process.IsValid());
  SignalChildren(signal_file.c_str());
  int exit_code;
  EXPECT_TRUE(process.WaitForExitWithTimeout(TestTimeouts::action_max_timeout(),
                                             &exit_code));
  remove(signal_file.c_str());
}

// Times out on Linux and Win, flakes on other platforms, http://crbug.com/95058
#define MAYBE_GetTerminationStatusExit DISABLED_GetTerminationStatusExit
TEST_F(ProcessUtilTest, MAYBE_GetTerminationStatusExit) {
  const std::string signal_file = GetSignalFilePath(kSignalFileSlow);
  remove(signal_file.c_str());
  Process process = SpawnChild("SlowChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_NORMAL_TERMINATION, status);
  EXPECT_EQ(kSuccess, exit_code);
  remove(signal_file.c_str());
}


// On Android SpawnProcess() doesn't use LaunchProcess() and doesn't support
// LaunchOptions::current_directory.
#if !BUILDFLAG(IS_ANDROID)
static void CheckCwdIsExpected(FilePath expected) {
  FilePath actual;
  CHECK(GetCurrentDirectory(&actual));
  actual = MakeAbsoluteFilePath(actual);
  CHECK(!actual.empty());

  CHECK_EQ(expected, actual);
}

// N.B. This test does extra work to check the cwd on multiple threads, because
// on macOS a per-thread cwd is set when using LaunchProcess().
MULTIPROCESS_TEST_MAIN(CheckCwdProcess) {
  // Get the expected cwd.
  FilePath temp_dir;
  CHECK(GetTempDir(&temp_dir));
  temp_dir = MakeAbsoluteFilePath(temp_dir);
  CHECK(!temp_dir.empty());

  // Test that the main thread has the right cwd.
  CheckCwdIsExpected(temp_dir);

  // Create a non-main thread.
  Thread thread("CheckCwdThread");
  thread.Start();
  auto task_runner = thread.task_runner();

  // A synchronization primitive used to wait for work done on the non-main
  // thread.
  WaitableEvent event(WaitableEvent::ResetPolicy::AUTOMATIC);
  RepeatingClosure signal_event =
      BindRepeating(&WaitableEvent::Signal, Unretained(&event));

  // Test that a non-main thread has the right cwd.
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, temp_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Get a new cwd for the process.
  const FilePath home_dir = PathService::CheckedGet(DIR_HOME);

  // Change the cwd on the secondary thread. IgnoreResult is used when setting
  // because it is checked immediately after.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(IgnoreResult(&SetCurrentDirectory), home_dir));
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, home_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Make sure the main thread sees the cwd from the secondary thread.
  CheckCwdIsExpected(home_dir);

  // Change the directory back on the main thread.
  CHECK(SetCurrentDirectory(temp_dir));
  CheckCwdIsExpected(temp_dir);

  // Ensure that the secondary thread sees the new cwd too.
  task_runner->PostTask(FROM_HERE, BindOnce(&CheckCwdIsExpected, temp_dir));
  task_runner->PostTask(FROM_HERE, signal_event);

  event.Wait();

  // Change the cwd on the secondary thread one more time and join the thread.
  task_runner->PostTask(FROM_HERE,
                        BindOnce(IgnoreResult(&SetCurrentDirectory), home_dir));
  thread.Stop();

  // Make sure that the main thread picked up the new cwd.
  CheckCwdIsExpected(home_dir);

  return kSuccess;
}

// A relative binary loader path is set in MSAN builds, so binaries must be
// run from the build directory.
#if !defined(MEMORY_SANITIZER)
TEST_F(ProcessUtilTest, CurrentDirectory) {
  // TODO(rickyz): Add support for passing arguments to multiprocess children,
  // then create a special directory for this test.
  FilePath tmp_dir;
  ASSERT_TRUE(GetTempDir(&tmp_dir));

  LaunchOptions options;
  options.current_directory = tmp_dir;

  Process process(SpawnChildWithOptions("CheckCwdProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}
#endif  // !defined(MEMORY_SANITIZER)
#endif  // !BUILDFLAG(IS_ANDROID)


#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
// This test is disabled on Mac, since it's flaky due to ReportCrash
// taking a variable amount of time to parse and load the debug and
// symbol data for this unit test's executable before firing the
// signal handler.
//
// TODO(gspencer): turn this test process into a very small program
// with no symbols (instead of using the multiprocess testing
// framework) to reduce the ReportCrash overhead.
//
// It is disabled on Android as MultiprocessTests are started as services that
// the framework restarts on crashes.
const char kSignalFileCrash[] = "CrashingChildProcess.die";

MULTIPROCESS_TEST_MAIN(CrashingChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileCrash).c_str());
#if BUILDFLAG(IS_POSIX)
  // Have to disable to signal handler for segv so we can get a crash
  // instead of an abnormal termination through the crash dump handler.
  ::signal(SIGSEGV, SIG_DFL);
#endif
  // Make this process have a segmentation fault.
  volatile int* oops = nullptr;
  *oops = 0xDEAD;
  return 1;
}

// This test intentionally crashes, so we don't need to run it under
// AddressSanitizer.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_GetTerminationStatusCrash DISABLED_GetTerminationStatusCrash
#else
#define MAYBE_GetTerminationStatusCrash GetTerminationStatusCrash
#endif
TEST_F(ProcessUtilTest, MAYBE_GetTerminationStatusCrash) {
  const std::string signal_file = GetSignalFilePath(kSignalFileCrash);
  remove(signal_file.c_str());
  Process process = SpawnChild("CrashingChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_CRASHED, status);

#if BUILDFLAG(IS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGSEGV, signal);
#endif

  // Reset signal handlers back to "normal".
  debug::EnableInProcessStackDumping();
  remove(signal_file.c_str());
}
#endif  // !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)

MULTIPROCESS_TEST_MAIN(KilledChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileKill).c_str());
#if BUILDFLAG(IS_POSIX)
  // Send a SIGKILL to this process, just like the OOM killer would.
  ::kill(getpid(), SIGKILL);
#endif
  return 1;
}

#if BUILDFLAG(IS_POSIX)
MULTIPROCESS_TEST_MAIN(TerminatedChildProcess) {
  WaitToDie(ProcessUtilTest::GetSignalFilePath(kSignalFileTerm).c_str());
  // Send a SIGTERM to this process.
  ::kill(getpid(), SIGTERM);
  return 1;
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(ProcessUtilTest, GetTerminationStatusSigKill) {
  const std::string signal_file = GetSignalFilePath(kSignalFileKill);
  remove(signal_file.c_str());
  Process process = SpawnChild("KilledChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM, status);
#else
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED, status);
#endif

#if BUILDFLAG(IS_POSIX)
  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGKILL, signal);
#endif
  remove(signal_file.c_str());
}

#if BUILDFLAG(IS_POSIX)
// TODO(crbug.com/42050610): Access to the process termination reason is not
// implemented in Fuchsia. Unix signals are not implemented in Fuchsia so this
// test might not be relevant anyway.
TEST_F(ProcessUtilTest, GetTerminationStatusSigTerm) {
  const std::string signal_file = GetSignalFilePath(kSignalFileTerm);
  remove(signal_file.c_str());
  Process process = SpawnChild("TerminatedChildProcess");
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_EQ(TERMINATION_STATUS_STILL_RUNNING,
            GetTerminationStatus(process.Handle(), &exit_code));
  EXPECT_EQ(kExpectedStillRunningExitCode, exit_code);

  SignalChildren(signal_file.c_str());
  exit_code = 42;
  TerminationStatus status =
      WaitForChildTermination(process.Handle(), &exit_code);
  EXPECT_EQ(TERMINATION_STATUS_PROCESS_WAS_KILLED, status);

  int signaled = WIFSIGNALED(exit_code);
  EXPECT_NE(0, signaled);
  int signal = WTERMSIG(exit_code);
  EXPECT_EQ(SIGTERM, signal);
  remove(signal_file.c_str());
}
#endif  // BUILDFLAG(IS_POSIX)

TEST_F(ProcessUtilTest, EnsureTerminationUndying) {
  test::TaskEnvironment task_environment;

  Process child_process = SpawnChild("process_util_test_never_die");
  ASSERT_TRUE(child_process.IsValid());

  EnsureProcessTerminated(child_process.Duplicate());

#if BUILDFLAG(IS_POSIX)
  errno = 0;
#endif  // BUILDFLAG(IS_POSIX)

  // Allow a generous timeout, to cope with slow/loaded test bots.
  bool did_exit = child_process.WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr);

#if BUILDFLAG(IS_POSIX)
  // Both EnsureProcessTerminated() and WaitForExitWithTimeout() will call
  // waitpid(). One will succeed, and the other will fail with ECHILD. If our
  // wait failed then check for ECHILD, and assumed |did_exit| in that case.
  did_exit = did_exit || (errno == ECHILD);
#endif  // BUILDFLAG(IS_POSIX)

  EXPECT_TRUE(did_exit);
}

MULTIPROCESS_TEST_MAIN(process_util_test_never_die) {
  while (true) {
    PlatformThread::Sleep(Seconds(500));
  }
}

TEST_F(ProcessUtilTest, EnsureTerminationGracefulExit) {
  test::TaskEnvironment task_environment;

  Process child_process = SpawnChild("process_util_test_die_immediately");
  ASSERT_TRUE(child_process.IsValid());

  // Wait for the child process to actually exit.
  child_process.Duplicate().WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr);

  EnsureProcessTerminated(child_process.Duplicate());

  // Verify that the process is really, truly gone.
  EXPECT_TRUE(child_process.WaitForExitWithTimeout(
      TestTimeouts::action_max_timeout(), nullptr));
}

MULTIPROCESS_TEST_MAIN(process_util_test_die_immediately) {
  return kSuccess;
}


TEST_F(ProcessUtilTest, GetAppOutput) {
  CommandLine command(test_helper_path_);
  command.AppendArg("hello");
  command.AppendArg("there");
  command.AppendArg("good");
  command.AppendArg("people");
  std::string output;
  EXPECT_TRUE(GetAppOutput(command, &output));
  EXPECT_EQ("hello there good people", output);
  output.clear();

  const char* kEchoMessage = "blah";
  command = CommandLine(test_helper_path_);
  command.AppendArg("-x");
  command.AppendArg("28");
  command.AppendArg(kEchoMessage);
  EXPECT_FALSE(GetAppOutput(command, &output));
  EXPECT_EQ(kEchoMessage, output);
}

TEST_F(ProcessUtilTest, GetAppOutputWithExitCode) {
  const char* kEchoMessage1 = "doge";
  int exit_code = -1;
  CommandLine command(test_helper_path_);
  command.AppendArg(kEchoMessage1);
  std::string output;
  EXPECT_TRUE(GetAppOutputWithExitCode(command, &output, &exit_code));
  EXPECT_EQ(kEchoMessage1, output);
  EXPECT_EQ(0, exit_code);
  output.clear();

  const char* kEchoMessage2 = "pupper";
  const int kExpectedExitCode = 42;
  command = CommandLine(test_helper_path_);
  command.AppendArg("-x");
  command.AppendArg(NumberToString(kExpectedExitCode));
  command.AppendArg(kEchoMessage2);
  EXPECT_TRUE(GetAppOutputWithExitCode(command, &output, &exit_code));
  EXPECT_EQ(kEchoMessage2, output);
  EXPECT_EQ(kExpectedExitCode, exit_code);
}

#if BUILDFLAG(IS_POSIX)

namespace {

// Returns the maximum number of files that a process can have open.
// Returns 0 on error.
int GetMaxFilesOpenInProcess() {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    return 0;
  }

  // rlim_t is a uint64_t - clip to maxint. We do this since FD #s are ints
  // which are all 32 bits on the supported platforms.
  rlim_t max_int = static_cast<rlim_t>(std::numeric_limits<int32_t>::max());
  if (rlim.rlim_cur > max_int) {
    return max_int;
  }

  return rlim.rlim_cur;
}

const int kChildPipe = 20;  // FD # for write end of pipe in child process.

#if BUILDFLAG(IS_APPLE)

// Declarations from 12.3 xnu-8020.101.4/bsd/sys/guarded.h (not in the SDK).
extern "C" {

using guardid_t = uint64_t;

#define GUARD_DUP (1u << 1)

int change_fdguard_np(int fd,
                      const guardid_t* guard,
                      unsigned int guardflags,
                      const guardid_t* nguard,
                      unsigned int nguardflags,
                      int* fdflagsp);

}  // extern "C"

// Attempt to set a file-descriptor guard on |fd|.  In case of success, remove
// it and return |true| to indicate that it can be guarded.  Returning |false|
// means either that |fd| is guarded by some other code, or more likely EBADF.
//
// Starting with 10.9, libdispatch began setting GUARD_DUP on a file descriptor.
// Unfortunately, it is spun up as part of +[NSApplication initialize], which is
// not really something that Chromium can avoid using on OSX.  See
// <http://crbug.com/338157>.  This function allows querying whether the file
// descriptor is guarded before attempting to close it.
bool CanGuardFd(int fd) {
  // Saves the original flags to reset later.
  int original_fdflags = 0;

  // This can be any value at all, it just has to match up between the two
  // calls.
  const guardid_t kGuard = 15;

  // Attempt to change the guard.  This can fail with EBADF if the file
  // descriptor is bad, or EINVAL if the fd already has a guard set.
  int ret =
      change_fdguard_np(fd, NULL, 0, &kGuard, GUARD_DUP, &original_fdflags);
  if (ret == -1) {
    return false;
  }

  // Remove the guard.  It should not be possible to fail in removing the guard
  // just added.
  ret = change_fdguard_np(fd, &kGuard, GUARD_DUP, NULL, 0, &original_fdflags);
  DPCHECK(ret == 0);

  return true;
}
#endif  // BUILDFLAG(IS_APPLE)

}  // namespace

MULTIPROCESS_TEST_MAIN(ProcessUtilsLeakFDChildProcess) {
  // This child process counts the number of open FDs, it then writes that
  // number out to a pipe connected to the parent.
  int num_open_files = 0;
  int write_pipe = kChildPipe;
  int max_files = GetMaxFilesOpenInProcess();
  for (int i = STDERR_FILENO + 1; i < max_files; i++) {
#if BUILDFLAG(IS_APPLE)
    // Ignore guarded or invalid file descriptors.
    if (!CanGuardFd(i)) {
      continue;
    }
#endif

    if (i != kChildPipe) {
      int fd;
      if ((fd = HANDLE_EINTR(dup(i))) != -1) {
        close(fd);
        num_open_files += 1;
      }
    }
  }

  int written =
      HANDLE_EINTR(write(write_pipe, &num_open_files, sizeof(num_open_files)));
  DCHECK_EQ(static_cast<size_t>(written), sizeof(num_open_files));
  int ret = IGNORE_EINTR(close(write_pipe));
  DPCHECK(ret == 0);

  return 0;
}

int ProcessUtilTest::CountOpenFDsInChild() {
  int fds[2];
  if (pipe(fds) < 0) {
    NOTREACHED();
  }

  LaunchOptions options;
  options.fds_to_remap.emplace_back(fds[1], kChildPipe);
  Process process =
      SpawnChildWithOptions("ProcessUtilsLeakFDChildProcess", options);
  CHECK(process.IsValid());
  int ret = IGNORE_EINTR(close(fds[1]));
  DPCHECK(ret == 0);

  // Read number of open files in client process from pipe;
  int num_open_files = -1;
  ssize_t bytes_read =
      HANDLE_EINTR(read(fds[0], &num_open_files, sizeof(num_open_files)));
  CHECK_EQ(bytes_read, static_cast<ssize_t>(sizeof(num_open_files)));

#if defined(THREAD_SANITIZER)
  // Compiler-based ThreadSanitizer makes this test slow.
  TimeDelta timeout = Seconds(3);
#else
  TimeDelta timeout = Seconds(1);
#endif
  int exit_code;
  CHECK(process.WaitForExitWithTimeout(timeout, &exit_code));
  ret = IGNORE_EINTR(close(fds[0]));
  DPCHECK(ret == 0);

  return num_open_files;
}

TEST_F(ProcessUtilTest, FDRemapping) {
  int fds_before = CountOpenFDsInChild();

  // Open some dummy fds to make sure they don't propagate over to the
  // child process.
  ScopedFD dev_null(open("/dev/null", O_RDONLY));

  DPCHECK(dev_null.get() > 0);
  int sockets[2];
  int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
  DPCHECK(ret == 0);

  int fds_after = CountOpenFDsInChild();

  ASSERT_EQ(fds_after, fds_before);

  ret = IGNORE_EINTR(close(sockets[0]));
  DPCHECK(ret == 0);
  ret = IGNORE_EINTR(close(sockets[1]));
  DPCHECK(ret == 0);
}

const char kPipeValue = '\xcc';
MULTIPROCESS_TEST_MAIN(ProcessUtilsVerifyStdio) {
  // Write to stdio so the parent process can observe output.
  CHECK_EQ(1, HANDLE_EINTR(write(STDOUT_FILENO, &kPipeValue, 1)));

  // Close all of the handles, to verify they are valid.
  CHECK_EQ(0, IGNORE_EINTR(close(STDIN_FILENO)));
  CHECK_EQ(0, IGNORE_EINTR(close(STDOUT_FILENO)));
  CHECK_EQ(0, IGNORE_EINTR(close(STDERR_FILENO)));
  return 0;
}

TEST_F(ProcessUtilTest, FDRemappingIncludesStdio) {
  ScopedFD some_fd(open("/dev/null", O_RDONLY));
  ASSERT_LT(2, some_fd.get());

  // Backup stdio and replace it with the write end of a pipe, for our
  // child process to inherit.
  int pipe_fds[2];
  int result = pipe(pipe_fds);
  ASSERT_EQ(0, result);
  int backup_stdio = HANDLE_EINTR(dup(STDOUT_FILENO));
  ASSERT_LE(0, backup_stdio);
  result = dup2(pipe_fds[1], STDOUT_FILENO);
  ASSERT_EQ(STDOUT_FILENO, result);

  // Launch the test process, which should inherit our pipe stdio.
  LaunchOptions options;
  options.fds_to_remap.emplace_back(some_fd.get(), some_fd.get());
  Process process = SpawnChildWithOptions("ProcessUtilsVerifyStdio", options);
  ASSERT_TRUE(process.IsValid());

  // Restore stdio, so we can output stuff.
  result = dup2(backup_stdio, STDOUT_FILENO);
  ASSERT_EQ(STDOUT_FILENO, result);

  // Close our copy of the write end of the pipe, so that the read()
  // from the other end will see EOF if it wasn't copied to the child.
  result = IGNORE_EINTR(close(pipe_fds[1]));
  ASSERT_EQ(0, result);

  result = IGNORE_EINTR(close(backup_stdio));
  ASSERT_EQ(0, result);
  // Also close the remapped descriptor.
  some_fd.reset();

  // Read from the pipe to verify that it is connected to the child
  // process' stdio.
  char buf[16] = {};
  EXPECT_EQ(1, HANDLE_EINTR(read(pipe_fds[0], buf, sizeof(buf))));
  EXPECT_EQ(kPipeValue, buf[0]);

  result = IGNORE_EINTR(close(pipe_fds[0]));
  ASSERT_EQ(0, result);

  int exit_code;
  ASSERT_TRUE(process.WaitForExitWithTimeout(Seconds(5), &exit_code));
  EXPECT_EQ(0, exit_code);
}


// There's no such thing as a parent process id on Fuchsia.
TEST_F(ProcessUtilTest, GetParentProcessId) {
  ProcessId ppid = GetParentProcessId(GetCurrentProcessHandle());
  EXPECT_EQ(ppid, static_cast<ProcessId>(getppid()));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_APPLE)
class WriteToPipeDelegate : public LaunchOptions::PreExecDelegate {
 public:
  explicit WriteToPipeDelegate(int fd) : fd_(fd) {}

  WriteToPipeDelegate(const WriteToPipeDelegate&) = delete;
  WriteToPipeDelegate& operator=(const WriteToPipeDelegate&) = delete;

  ~WriteToPipeDelegate() override = default;
  void RunAsyncSafe() override {
    RAW_CHECK(HANDLE_EINTR(write(fd_, &kPipeValue, 1)) == 1);
    RAW_CHECK(IGNORE_EINTR(close(fd_)) == 0);
  }

 private:
  int fd_;
};

TEST_F(ProcessUtilTest, PreExecHook) {
  int pipe_fds[2];
  ASSERT_EQ(0, pipe(pipe_fds));

  ScopedFD read_fd(pipe_fds[0]);
  ScopedFD write_fd(pipe_fds[1]);

  WriteToPipeDelegate write_to_pipe_delegate(write_fd.get());
  LaunchOptions options;
  options.fds_to_remap.emplace_back(write_fd.get(), write_fd.get());
  options.pre_exec_delegate = &write_to_pipe_delegate;
  Process process(SpawnChildWithOptions("SimpleChildProcess", options));
  ASSERT_TRUE(process.IsValid());

  write_fd.reset();
  char c;
  ASSERT_EQ(1, HANDLE_EINTR(read(read_fd.get(), &c, 1)));
  EXPECT_EQ(c, kPipeValue);

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(0, exit_code);
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_APPLE)

#endif  // BUILDFLAG(IS_POSIX)

// There's no such thing as a parent process id on Fuchsia.
TEST_F(ProcessUtilTest, GetParentProcessId2) {
  ProcessId id1 = GetCurrentProcId();
  Process process = SpawnChild("SimpleChildProcess");
  ASSERT_TRUE(process.IsValid());
  ProcessId ppid = GetParentProcessId(process.Handle());
  EXPECT_EQ(ppid, id1);
}

namespace {

std::string TestLaunchProcess(const CommandLine& cmdline,
                              const EnvironmentMap& env_changes,
                              const bool clear_environment,
                              const int clone_flags) {
  LaunchOptions options;
  options.wait = true;
  options.environment = env_changes;
  options.clear_environment = clear_environment;

  int fds[2];
  PCHECK(pipe(fds) == 0);
  File read_pipe(fds[0]);
  File write_pipe(fds[1]);
  options.fds_to_remap.emplace_back(fds[1], STDOUT_FILENO);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  options.clone_flags = clone_flags;
#else
  CHECK_EQ(0, clone_flags);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  EXPECT_TRUE(LaunchProcess(cmdline, options).IsValid());
  write_pipe.Close();

  uint8_t buf[512];
  std::optional<size_t> n_opt = read_pipe.ReadAtCurrentPos(buf);
  PCHECK(n_opt.has_value());
  return std::string(as_string_view(span(buf).first(n_opt.value())));
}

const char kLargeString[] =
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789"
    "0123456789012345678901234567890123456789012345678901234567890123456789";

}  // namespace

TEST_F(ProcessUtilTest, LaunchProcess) {
  const int no_clone_flags = 0;
  const bool no_clear_environ = false;
  const FilePath::CharType kBaseTest[] = FILE_PATH_LITERAL("BASE_TEST");
  const CommandLine kPrintEnvCommand(CommandLine::StringVector(
      {test_helper_path_.value(), FILE_PATH_LITERAL("-e"), kBaseTest}));
  std::unique_ptr<Environment> env = Environment::Create();

  EnvironmentMap env_changes;
  env_changes[kBaseTest] = FILE_PATH_LITERAL("bar");
  EXPECT_EQ("bar", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                     no_clear_environ, no_clone_flags));
  env_changes.clear();

  EXPECT_TRUE(env->SetVar("BASE_TEST", "testing"));
  EXPECT_EQ("testing", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                         no_clear_environ, no_clone_flags));

  env_changes[kBaseTest] = FilePath::StringType();
  EXPECT_EQ("", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                  no_clear_environ, no_clone_flags));

  env_changes[kBaseTest] = FILE_PATH_LITERAL("foo");
  EXPECT_EQ("foo", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                     no_clear_environ, no_clone_flags));

  env_changes.clear();
  EXPECT_TRUE(env->SetVar("BASE_TEST", kLargeString));
  EXPECT_EQ(std::string(kLargeString),
            TestLaunchProcess(kPrintEnvCommand, env_changes, no_clear_environ,
                              no_clone_flags));

  env_changes[kBaseTest] = FILE_PATH_LITERAL("wibble");
  EXPECT_EQ("wibble", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                        no_clear_environ, no_clone_flags));

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // Test a non-trival value for clone_flags.
  EXPECT_EQ("wibble", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                        no_clear_environ, CLONE_FS));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

  EXPECT_EQ("wibble",
            TestLaunchProcess(kPrintEnvCommand, env_changes,
                              true /* clear_environ */, no_clone_flags));
  env_changes.clear();
  EXPECT_EQ("", TestLaunchProcess(kPrintEnvCommand, env_changes,
                                  true /* clear_environ */, no_clone_flags));
}


#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
MULTIPROCESS_TEST_MAIN(CheckPidProcess) {
  const pid_t kInitPid = 1;
  const pid_t pid = syscall(__NR_getpid);
  CHECK(pid == kInitPid);
  CHECK(getpid() == pid);
  return kSuccess;
}

#if defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)
TEST_F(ProcessUtilTest, CloneFlags) {
  if (!PathExists(FilePath("/proc/self/ns/user")) ||
      !PathExists(FilePath("/proc/self/ns/pid"))) {
    // User or PID namespaces are not supported.
    return;
  }

  LaunchOptions options;
  options.clone_flags = CLONE_NEWUSER | CLONE_NEWPID;

  Process process(SpawnChildWithOptions("CheckPidProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = 42;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_EQ(kSuccess, exit_code);
}
#endif  // defined(CLONE_NEWUSER) && defined(CLONE_NEWPID)

TEST(ForkWithFlagsTest, UpdatesPidCache) {
  // Warm up the libc pid cache, if there is one.
  ASSERT_EQ(syscall(__NR_getpid), getpid());

  pid_t ctid = 0;
  const pid_t pid = ForkWithFlags(SIGCHLD | CLONE_CHILD_SETTID, nullptr, &ctid);
  if (pid == 0) {
    // In child.  Check both the raw getpid syscall and the libc getpid wrapper
    // (which may rely on a pid cache).
    RAW_CHECK(syscall(__NR_getpid) == ctid);
    RAW_CHECK(getpid() == ctid);
    _exit(kSuccess);
  }

  ASSERT_NE(-1, pid);
  int status = 42;
  ASSERT_EQ(pid, HANDLE_EINTR(waitpid(pid, &status, 0)));
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(kSuccess, WEXITSTATUS(status));
}

TEST_F(ProcessUtilTest, InvalidCurrentDirectory) {
  LaunchOptions options;
  options.current_directory = FilePath("/dev/null");

  Process process(SpawnChildWithOptions("SimpleChildProcess", options));
  ASSERT_TRUE(process.IsValid());

  int exit_code = kSuccess;
  EXPECT_TRUE(process.WaitForExit(&exit_code));
  EXPECT_NE(kSuccess, exit_code);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)

constexpr MachPortsForRendezvous::key_type kTestChildRendezvousKey = 'test';

MULTIPROCESS_TEST_MAIN(
    ProcessUtilTest_ProcessRequirement_ChildFailedValidation) {
  auto* rendezvous_client = MachPortRendezvousClient::GetInstance();
  CHECK(rendezvous_client);
  // The parent process specifies a requirement that does not matchy any
  // process. No ports will be available to this process.
  CHECK_EQ(0u, rendezvous_client->GetPortCount());
  return 0;
}

// Tests that a `ProcessRequirement` passed via `LaunchOptions` is applied
// during Mach port rendezvous with a child process. Tests of the validation
// behavior can be found in //base/apple/mach_port_rendezvous_unittest.cc.
TEST_F(ProcessUtilTest, ProcessRequirement) {
  // TODO(crbug.com/362302761): Remove once peer validation is enforced by
  // default.
  test::ScopedFeatureList scoped_feature_list(
      kMachPortRendezvousEnforcePeerRequirements);

  apple::ScopedMachReceiveRight port;
  kern_return_t kr =
      mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
                         apple::ScopedMachReceiveRight::Receiver(port).get());
  ASSERT_EQ(kr, KERN_SUCCESS);
  MachRendezvousPort rendezvous_port(port.get(), MACH_MSG_TYPE_MAKE_SEND);

  // One Mach port is registered, but the process requirement should prevent
  // the child from retrieving it. Test assertions are in the child process,
  // above.
  LaunchOptions options;
  options.mach_ports_for_rendezvous[kTestChildRendezvousKey] = rendezvous_port;
  options.process_requirement =
      mac::ProcessRequirement::NeverMatchesForTesting();

  Process child = SpawnChildWithOptions(
      "ProcessUtilTest_ProcessRequirement_ChildFailedValidation", options);

  int exit_code;
  ASSERT_TRUE(WaitForMultiprocessTestChildExit(
      child, TestTimeouts::action_timeout(), &exit_code));

  // The child exiting successfully indicates that its test assertions held.
  EXPECT_EQ(0, exit_code);
}

#endif

}  // namespace base
