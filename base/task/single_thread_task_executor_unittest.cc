// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/pending_task.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_observer.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/java_handler_thread.h"
#include "base/android/jni_android.h"
#include "base/test/android/java_handler_thread_helpers.h"
#endif

using ::testing::IsNull;
using ::testing::NotNull;

namespace base {

// TODO(darin): Platform-specific MessageLoop tests should be grouped together
// to avoid chopping this file up with so many #ifdefs.

namespace {

class Foo : public RefCounted<Foo> {
 public:
  Foo() = default;

  Foo(const Foo&) = delete;
  Foo& operator=(const Foo&) = delete;

  void Test0() { ++test_count_; }

  void Test1ConstRef(const std::string& a) {
    ++test_count_;
    result_.append(a);
  }

  void Test1Ptr(std::string* a) {
    ++test_count_;
    result_.append(*a);
  }

  void Test1Int(int a) { test_count_ += a; }

  void Test2Ptr(std::string* a, std::string* b) {
    ++test_count_;
    result_.append(*a);
    result_.append(*b);
  }

  void Test2Mixed(const std::string& a, std::string* b) {
    ++test_count_;
    result_.append(a);
    result_.append(*b);
  }

  int test_count() const { return test_count_; }
  const std::string& result() const LIFETIME_BOUND { return result_; }

 private:
  friend class RefCounted<Foo>;

  ~Foo() = default;

  int test_count_ = 0;
  std::string result_;
};

// This function runs slowly to simulate a large amount of work being done.
static void SlowFunc(TimeDelta pause,
                     int* quit_counter,
                     base::OnceClosure quit_closure) {
  PlatformThread::Sleep(pause);
  if (--(*quit_counter) == 0) {
    std::move(quit_closure).Run();
  }
}

// This function records the time when Run was called in a Time object, which is
// useful for building a variety of SingleThreadTaskExecutor tests.
static void RecordRunTimeFunc(TimeTicks* run_time,
                              int* quit_counter,
                              base::OnceClosure quit_closure) {
  *run_time = TimeTicks::Now();

  // Cause our Run function to take some time to execute.  As a result we can
  // count on subsequent RecordRunTimeFunc()s running at a future time,
  // without worry about the resolution of our system clock being an issue.
  SlowFunc(Milliseconds(10), quit_counter, std::move(quit_closure));
}

enum TaskType {
  MESSAGEBOX,
  ENDDIALOG,
  RECURSIVE,
  TIMEDMESSAGELOOP,
  QUITMESSAGELOOP,
  ORDERED,
  PUMPS,
  SLEEP,
  RUNS,
};

// Saves the order in which the tasks executed.
struct TaskItem {
  TaskItem(TaskType t, int c, bool s) : type(t), cookie(c), start(s) {}

  TaskType type;
  int cookie;
  bool start;

  bool operator==(const TaskItem& other) const {
    return type == other.type && cookie == other.cookie && start == other.start;
  }
};

std::ostream& operator<<(std::ostream& os, TaskType type) {
  switch (type) {
    case MESSAGEBOX:
      os << "MESSAGEBOX";
      break;
    case ENDDIALOG:
      os << "ENDDIALOG";
      break;
    case RECURSIVE:
      os << "RECURSIVE";
      break;
    case TIMEDMESSAGELOOP:
      os << "TIMEDMESSAGELOOP";
      break;
    case QUITMESSAGELOOP:
      os << "QUITMESSAGELOOP";
      break;
    case ORDERED:
      os << "ORDERED";
      break;
    case PUMPS:
      os << "PUMPS";
      break;
    case SLEEP:
      os << "SLEEP";
      break;
    default:
      NOTREACHED();
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const TaskItem& item) {
  if (item.start) {
    return os << item.type << " " << item.cookie << " starts";
  }
  return os << item.type << " " << item.cookie << " ends";
}

class TaskList {
 public:
  void RecordStart(TaskType type, int cookie) {
    TaskItem item(type, cookie, true);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  void RecordEnd(TaskType type, int cookie) {
    TaskItem item(type, cookie, false);
    DVLOG(1) << item;
    task_list_.push_back(item);
  }

  size_t Size() { return task_list_.size(); }

  TaskItem Get(int n) { return task_list_[n]; }

 private:
  std::vector<TaskItem> task_list_;
};

class DummyTaskObserver : public TaskObserver {
 public:
  explicit DummyTaskObserver(int num_tasks)
      : num_tasks_started_(0), num_tasks_processed_(0), num_tasks_(num_tasks) {}

  DummyTaskObserver(int num_tasks, int num_tasks_started)
      : num_tasks_started_(num_tasks_started),
        num_tasks_processed_(0),
        num_tasks_(num_tasks) {}

  DummyTaskObserver(const DummyTaskObserver&) = delete;
  DummyTaskObserver& operator=(const DummyTaskObserver&) = delete;

  ~DummyTaskObserver() override = default;

  void WillProcessTask(const PendingTask& pending_task,
                       bool /* was_blocked_or_low_priority */) override {
    num_tasks_started_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_ + 1);
  }

  void DidProcessTask(const PendingTask& pending_task) override {
    num_tasks_processed_++;
    EXPECT_LE(num_tasks_started_, num_tasks_);
    EXPECT_EQ(num_tasks_started_, num_tasks_processed_);
  }

  int num_tasks_started() const { return num_tasks_started_; }
  int num_tasks_processed() const { return num_tasks_processed_; }

 private:
  int num_tasks_started_;
  int num_tasks_processed_;
  const int num_tasks_;
};

// A method which reposts itself |depth| times.
void RecursiveFunc(TaskList* order, int cookie, int depth) {
  order->RecordStart(RECURSIVE, cookie);
  if (depth > 0) {
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&RecursiveFunc, order, cookie, depth - 1));
  }
  order->RecordEnd(RECURSIVE, cookie);
}

void QuitFunc(TaskList* order, int cookie, base::OnceClosure quit_closure) {
  order->RecordStart(QUITMESSAGELOOP, cookie);
  std::move(quit_closure).Run();
  order->RecordEnd(QUITMESSAGELOOP, cookie);
}

void Post128KTasksThenQuit(SingleThreadTaskRunner* executor_task_runner,
                           TimeTicks begin_ticks,
                           TimeTicks last_post_ticks,
                           TimeDelta slowest_delay,
                           OnceClosure on_done,
                           int num_posts_done = 0) {
  const int kNumTimes = 128000;

  // Tasks should be running on a decent heart beat. Some platforms/bots however
  // have a hard time posting+running *all* tasks before test timeout, add
  // detailed logging for diagnosis where this flakes.
  const auto now = TimeTicks::Now();
  const auto scheduling_delay = now - last_post_ticks;
  if (scheduling_delay > slowest_delay) {
    slowest_delay = scheduling_delay;
  }

  if (num_posts_done == kNumTimes) {
    std::move(on_done).Run();
    return;
  } else if (now - begin_ticks >= TestTimeouts::action_max_timeout()) {
    ADD_FAILURE() << "Couldn't run all tasks."
                  << "\nNumber of tasks remaining: "
                  << kNumTimes - num_posts_done
                  << "\nSlowest scheduling delay: " << slowest_delay
                  << "\nAverage per task: "
                  << (now - begin_ticks) / num_posts_done;
    std::move(on_done).Run();
    return;
  }

  executor_task_runner->PostTask(
      FROM_HERE,
      BindOnce(&Post128KTasksThenQuit, Unretained(executor_task_runner),
               begin_ticks, now, slowest_delay, std::move(on_done),
               num_posts_done + 1));
}

}  // namespace

//-----------------------------------------------------------------------------
// Each test is run against each type of SingleThreadTaskExecutor.  That way we
// are sure that SingleThreadTaskExecutor works properly in all configurations.
// Of course, in some cases, a unit test may only be for a particular type of
// loop.

class SingleThreadTaskExecutorTypedTest
    : public ::testing::TestWithParam<MessagePumpType> {
 public:
  SingleThreadTaskExecutorTypedTest() = default;

  SingleThreadTaskExecutorTypedTest(const SingleThreadTaskExecutorTypedTest&) =
      delete;
  SingleThreadTaskExecutorTypedTest& operator=(
      const SingleThreadTaskExecutorTypedTest&) = delete;

  ~SingleThreadTaskExecutorTypedTest() override = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<MessagePumpType> param_info) {
    switch (param_info.param) {
      case MessagePumpType::DEFAULT:
        return "default_pump";
      case MessagePumpType::IO:
        return "IO_pump";
      case MessagePumpType::UI:
        return "UI_pump";
      case MessagePumpType::CUSTOM:
        break;
#if BUILDFLAG(IS_ANDROID)
      case MessagePumpType::JAVA:
        break;
#endif  // BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_APPLE)
      case MessagePumpType::NS_RUNLOOP:
        break;
#endif  // BUILDFLAG(IS_APPLE)
    }
    NOTREACHED();
  }
};

TEST_P(SingleThreadTaskExecutorTypedTest, PostTask) {
  SingleThreadTaskExecutor executor(GetParam());
  base::RunLoop loop;
  // Add tests to message loop
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a"), b("b"), c("c"), d("d");
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test0, foo));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1Ptr, foo, &b));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1Int, foo, 100));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Ptr, foo, &a, &c));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test2Mixed, foo, a, &d));
  // After all tests, post a message that will shut down the message loop
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(loop.QuitWhenIdleClosure()));

  // Now kick things off
  loop.Run();

  EXPECT_EQ(foo->test_count(), 105);
  EXPECT_EQ(foo->result(), "abacad");
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_Basic) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that PostDelayedTask results in a delayed task.

  const TimeDelta kDelay = Milliseconds(100);

  int num_tasks = 1;
  TimeTicks run_time;
  base::RunLoop loop;
  TimeTicks time_before_run = TimeTicks::Now();
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks,
               loop.QuitWhenIdleClosure()),
      kDelay);
  loop.Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);
  EXPECT_LT(kDelay, time_after_run - time_before_run);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InDelayOrder) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that two tasks with different delays run in the right order.
  int num_tasks = 2;
  TimeTicks run_time1, run_time2;
  base::RunLoop loop;
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Milliseconds(200));
  // If we get a large pause in execution (due to a context switch) here, this
  // test could fail.
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Milliseconds(10));

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 < run_time1);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that two tasks with the same delay run in the order in which they
  // were posted.
  //
  // NOTE: This is actually an approximate test since the API only takes a
  // "delay" parameter, so we are not exactly simulating two tasks that get
  // posted at the exact same time.  It would be nice if the API allowed us to
  // specify the desired run time.

  const TimeDelta kDelay = Milliseconds(100);

  int num_tasks = 2;
  TimeTicks run_time1, run_time2;
  base::RunLoop loop;
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks,
               loop.QuitWhenIdleClosure()),
      kDelay);
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks,
               loop.QuitWhenIdleClosure()),
      kDelay);

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time1 < run_time2);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder_2) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that a delayed task still runs after a normal tasks even if the
  // normal tasks take a long time to run.

  const TimeDelta kPause = Milliseconds(50);

  int num_tasks = 2;
  TimeTicks run_time;
  base::RunLoop loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(&SlowFunc, kPause, &num_tasks, loop.QuitWhenIdleClosure()));
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Milliseconds(10));

  TimeTicks time_before_run = TimeTicks::Now();
  loop.Run();
  TimeTicks time_after_run = TimeTicks::Now();

  EXPECT_EQ(0, num_tasks);

  EXPECT_LT(kPause, time_after_run - time_before_run);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_InPostOrder_3) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that a delayed task still runs after a pile of normal tasks.  The key
  // difference between this test and the previous one is that here we return
  // the SingleThreadTaskExecutor a lot so we give the SingleThreadTaskExecutor
  // plenty of opportunities to maybe run the delayed task.  It should know not
  // to do so until the delayed task's delay has passed.

  int num_tasks = 11;
  TimeTicks run_time1, run_time2;
  base::RunLoop loop;
  // Clutter the ML with tasks.
  for (int i = 1; i < num_tasks; ++i) {
    executor.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks,
                            loop.QuitWhenIdleClosure()));
  }

  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Milliseconds(1));

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  EXPECT_TRUE(run_time2 > run_time1);
}

TEST_P(SingleThreadTaskExecutorTypedTest, PostDelayedTask_SharedTimer) {
  SingleThreadTaskExecutor executor(GetParam());

  // Test that the interval of the timer, used to run the next delayed task, is
  // set to a value corresponding to when the next delayed task should run.

  // By setting num_tasks to 1, we ensure that the first task to run causes the
  // run loop to exit.
  int num_tasks = 1;
  TimeTicks run_time1, run_time2;
  base::RunLoop loop;
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time1, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Seconds(1000));
  executor.task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&RecordRunTimeFunc, &run_time2, &num_tasks,
               loop.QuitWhenIdleClosure()),
      Milliseconds(10));

  TimeTicks start_time = TimeTicks::Now();

  loop.Run();
  EXPECT_EQ(0, num_tasks);

  // Ensure that we ran in far less time than the slower timer.
  TimeDelta total_time = TimeTicks::Now() - start_time;
  EXPECT_GT(5000, total_time.InMilliseconds());

  // In case both timers somehow run at nearly the same time, sleep a little
  // and then run all pending to force them both to have run.  This is just
  // encouraging flakiness if there is any.
  PlatformThread::Sleep(Milliseconds(100));
  RunLoop().RunUntilIdle();

  EXPECT_TRUE(run_time1.is_null());
  EXPECT_FALSE(run_time2.is_null());
}

namespace {

// This is used to inject a test point for recording the destructor calls for
// Closure objects send to MessageLoop::PostTask(). It is awkward usage since we
// are trying to hook the actual destruction, which is not a common operation.
class RecordDeletionProbe : public RefCounted<RecordDeletionProbe> {
 public:
  RecordDeletionProbe(RecordDeletionProbe* post_on_delete, bool* was_deleted)
      : post_on_delete_(post_on_delete), was_deleted_(was_deleted) {}
  void Run() {}

 private:
  friend class RefCounted<RecordDeletionProbe>;

  ~RecordDeletionProbe() {
    *was_deleted_ = true;
    if (post_on_delete_.get()) {
      SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, BindOnce(&RecordDeletionProbe::Run, post_on_delete_));
    }
  }

  scoped_refptr<RecordDeletionProbe> post_on_delete_;
  raw_ptr<bool> was_deleted_;
};

}  // namespace

/* TODO(darin): SingleThreadTaskExecutor does not support deleting all tasks in
 */
/* the destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(SingleThreadTaskExecutorTypedTest, DISABLED_EnsureDeletion) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  {
    SingleThreadTaskExecutor executor(GetParam());
    executor.task_runner()->PostTask(
        FROM_HERE, BindOnce(&RecordDeletionProbe::Run,
                            new RecordDeletionProbe(nullptr, &a_was_deleted)));
    // TODO(ajwong): Do we really need 1000ms here?
    executor.task_runner()->PostDelayedTask(
        FROM_HERE,
        BindOnce(&RecordDeletionProbe::Run,
                 new RecordDeletionProbe(nullptr, &b_was_deleted)),
        Milliseconds(1000));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
}

/* TODO(darin): SingleThreadTaskExecutor does not support deleting all tasks in
 */
/* the destructor. */
/* Fails, http://crbug.com/50272. */
TEST_P(SingleThreadTaskExecutorTypedTest, DISABLED_EnsureDeletion_Chain) {
  bool a_was_deleted = false;
  bool b_was_deleted = false;
  bool c_was_deleted = false;
  {
    SingleThreadTaskExecutor executor(GetParam());
    // The scoped_refptr for each of the below is held either by the chained
    // RecordDeletionProbe, or the bound RecordDeletionProbe::Run() callback.
    RecordDeletionProbe* a = new RecordDeletionProbe(nullptr, &a_was_deleted);
    RecordDeletionProbe* b = new RecordDeletionProbe(a, &b_was_deleted);
    RecordDeletionProbe* c = new RecordDeletionProbe(b, &c_was_deleted);
    executor.task_runner()->PostTask(FROM_HERE,
                                     BindOnce(&RecordDeletionProbe::Run, c));
  }
  EXPECT_TRUE(a_was_deleted);
  EXPECT_TRUE(b_was_deleted);
  EXPECT_TRUE(c_was_deleted);
}

namespace {

void NestingFunc(int* depth, base::OnceClosure quit_closure) {
  if (*depth > 0) {
    *depth -= 1;
    base::RunLoop loop1{base::RunLoop::Type::kNestableTasksAllowed};
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(&NestingFunc, depth, loop1.QuitWhenIdleClosure()));

    loop1.Run();
  }
  std::move(quit_closure).Run();
}

}  // namespace

TEST_P(SingleThreadTaskExecutorTypedTest, Nesting) {
  SingleThreadTaskExecutor executor(GetParam());
  base::RunLoop loop;
  int depth = 50;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&NestingFunc, &depth, loop.QuitWhenIdleClosure()));
  loop.Run();
  EXPECT_EQ(depth, 0);
}

TEST_P(SingleThreadTaskExecutorTypedTest, Recursive) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;
  base::RunLoop loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 1, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&RecursiveFunc, &order, 2, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 3, loop.QuitWhenIdleClosure()));

  loop.Run();

  // FIFO order.
  ASSERT_EQ(14U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
  EXPECT_EQ(order.Get(6), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(7), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(RECURSIVE, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(RECURSIVE, 1, true));
  EXPECT_EQ(order.Get(11), TaskItem(RECURSIVE, 1, false));
  EXPECT_EQ(order.Get(12), TaskItem(RECURSIVE, 2, true));
  EXPECT_EQ(order.Get(13), TaskItem(RECURSIVE, 2, false));
}

namespace {

void OrderedFunc(TaskList* order, int cookie) {
  order->RecordStart(ORDERED, cookie);
  order->RecordEnd(ORDERED, cookie);
}

}  // namespace

// Tests that non nestable tasks run in FIFO if there are no nested loops.
TEST_P(SingleThreadTaskExecutorTypedTest, NonNestableWithNoNesting) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;
  base::RunLoop loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 3, loop.QuitWhenIdleClosure()));
  loop.Run();

  // FIFO order.
  ASSERT_EQ(6U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(3), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(4), TaskItem(QUITMESSAGELOOP, 3, true));
  EXPECT_EQ(order.Get(5), TaskItem(QUITMESSAGELOOP, 3, false));
}

namespace {

void FuncThatPumps(TaskList* order, int cookie) {
  order->RecordStart(PUMPS, cookie);
  RunLoop(RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
  order->RecordEnd(PUMPS, cookie);
}

void SleepFunc(TaskList* order, int cookie, TimeDelta delay) {
  order->RecordStart(SLEEP, cookie);
  PlatformThread::Sleep(delay);
  order->RecordEnd(SLEEP, cookie);
}

}  // namespace

// Tests that non nestable tasks don't run when there's code in the call stack.
TEST_P(SingleThreadTaskExecutorTypedTest, NonNestableDelayedInNestedLoop) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;
  base::RunLoop loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatPumps, &order, 1));
  SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 3));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&SleepFunc, &order, 4, Milliseconds(50)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 5));
  SingleThreadTaskRunner::GetCurrentDefault()->PostNonNestableTask(
      FROM_HERE, BindOnce(&QuitFunc, &order, 6, loop.QuitWhenIdleClosure()));

  loop.Run();

  // FIFO order.
  ASSERT_EQ(12U, order.Size());
  EXPECT_EQ(order.Get(0), TaskItem(PUMPS, 1, true));
  EXPECT_EQ(order.Get(1), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(2), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(3), TaskItem(SLEEP, 4, true));
  EXPECT_EQ(order.Get(4), TaskItem(SLEEP, 4, false));
  EXPECT_EQ(order.Get(5), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(6), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(7), TaskItem(PUMPS, 1, false));
  EXPECT_EQ(order.Get(8), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(9), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(10), TaskItem(QUITMESSAGELOOP, 6, true));
  EXPECT_EQ(order.Get(11), TaskItem(QUITMESSAGELOOP, 6, false));
}

namespace {

void FuncThatRuns(TaskList* order, int cookie, RunLoop* run_loop) {
  order->RecordStart(RUNS, cookie);
  run_loop->Run();
  order->RecordEnd(RUNS, cookie);
}

void FuncThatQuitsNow(base::OnceClosure quit_closure) {
  std::move(quit_closure).Run();
}

}  // namespace

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, QuitNow) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
  RunLoop outer_run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow, nested_run_loop.QuitClosure()));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 3));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow, outer_run_loop.QuitClosure()));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 4));  // never runs

  outer_run_loop.Run();

  ASSERT_EQ(6U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitTop) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitNested) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Quits current loop and immediately runs a nested loop.
void QuitAndRunNestedLoop(TaskList* order,
                          int cookie,
                          RunLoop* outer_run_loop,
                          RunLoop* nested_run_loop) {
  order->RecordStart(RUNS, cookie);
  outer_run_loop->Quit();
  nested_run_loop->Run();
  order->RecordEnd(RUNS, cookie);
}

// Test that we can run nested loop after quitting the current one.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopNestedAfterQuit) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop;

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&QuitAndRunNestedLoop, &order, 1, &outer_run_loop,
                          &nested_run_loop));

  outer_run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitBogus) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
  RunLoop bogus_run_loop;

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, bogus_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());

  outer_run_loop.Run();

  ASSERT_EQ(4U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit only quits the corresponding MessageLoop::Run.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitDeep) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop outer_run_loop;
  RunLoop nested_loop1(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop2(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop3(RunLoop::Type::kNestableTasksAllowed);
  RunLoop nested_loop4(RunLoop::Type::kNestableTasksAllowed);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_loop1)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 2, Unretained(&nested_loop2)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 3, Unretained(&nested_loop3)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatRuns, &order, 4, Unretained(&nested_loop4)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 5));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, outer_run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 6));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_loop1.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 7));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_loop2.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 8));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_loop3.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 9));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_loop4.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 10));

  outer_run_loop.Run();

  ASSERT_EQ(18U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 5, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 6, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 7, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 8, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 9, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 4, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works before RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderBefore) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop run_loop;

  run_loop.Quit();

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));  // never runs
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow,
                          run_loop.QuitClosure()));  // never runs

  run_loop.Run();

  ASSERT_EQ(0U, order.Size());
}

// Tests RunLoopQuit works during RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderDuring) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop run_loop;

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 1));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        run_loop.QuitClosure());
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));  // never runs
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow,
                          run_loop.QuitClosure()));  // never runs

  run_loop.Run();

  ASSERT_EQ(2U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 1, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Tests RunLoopQuit works after RunWithID.
TEST_P(SingleThreadTaskExecutorTypedTest, RunLoopQuitOrderAfter) {
  SingleThreadTaskExecutor executor(GetParam());

  TaskList order;

  RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
  RunLoop outer_run_loop;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      BindOnce(&FuncThatRuns, &order, 1, Unretained(&nested_run_loop)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 2));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow, nested_run_loop.QuitClosure()));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 3));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, nested_run_loop.QuitClosure());  // has no affect
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&OrderedFunc, &order, 4));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&FuncThatQuitsNow, outer_run_loop.QuitClosure()));

  outer_run_loop.Run();

  ASSERT_EQ(8U, order.Size());
  int task_index = 0;
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 2, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(RUNS, 1, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 3, false));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, true));
  EXPECT_EQ(order.Get(task_index++), TaskItem(ORDERED, 4, false));
  EXPECT_EQ(static_cast<size_t>(task_index), order.Size());
}

// Regression test for crbug.com/170904 where posting tasks recursively caused
// the message loop to hang in MessagePumpGLib, due to the buffer of the
// internal pipe becoming full. Test all SingleThreadTaskExecutor types to
// ensure this issue does not exist in other MessagePumps.
//
// On Linux, the pipe buffer size is 64KiB by default. The bug caused one byte
// accumulated in the pipe per two posts, so we should repeat 128K times to
// reproduce the bug.
#define MAYBE_RecursivePostsDoNotFloodPipe RecursivePostsDoNotFloodPipe
TEST_P(SingleThreadTaskExecutorTypedTest, MAYBE_RecursivePostsDoNotFloodPipe) {
  SingleThreadTaskExecutor executor(GetParam());
  const auto begin_ticks = TimeTicks::Now();
  RunLoop run_loop;
  Post128KTasksThenQuit(executor.task_runner().get(), begin_ticks, begin_ticks,
                        TimeDelta(), run_loop.QuitClosure());
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest,
       ApplicationTasksAllowedInNativeNestedLoopAtTopLevel) {
  SingleThreadTaskExecutor executor(GetParam());
  EXPECT_TRUE(
      CurrentThread::Get()->ApplicationTasksAllowedInNativeNestedLoop());
}

// Nestable tasks shouldn't be allowed to run reentrantly by default (regression
// test for https://crbug.com/754112).
TEST_P(SingleThreadTaskExecutorTypedTest, NestableTasksDisallowedByDefault) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            EXPECT_FALSE(CurrentThread::Get()
                             ->ApplicationTasksAllowedInNativeNestedLoop());
            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest,
       NestableTasksProcessedWhenRunLoopAllows) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            // This test would hang if this RunLoop wasn't of type
            // kNestableTasksAllowed (i.e. this is testing that this is
            // processed and doesn't hang).
            RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);
            SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
                FROM_HERE,
                BindOnce(
                    [](RunLoop* nested_run_loop) {
                      // Each additional layer of application task nesting
                      // requires its own allowance. The kNestableTasksAllowed
                      // RunLoop allowed this task to be processed but further
                      // nestable tasks are by default disallowed from this
                      // layer.
                      EXPECT_FALSE(
                          CurrentThread::Get()
                              ->ApplicationTasksAllowedInNativeNestedLoop());
                      nested_run_loop->Quit();
                    },
                    Unretained(&nested_run_loop)));
            nested_run_loop.Run();

            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

TEST_P(SingleThreadTaskExecutorTypedTest, IsIdleForTesting) {
  SingleThreadTaskExecutor executor(GetParam());
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
  executor.task_runner()->PostTask(FROM_HERE, BindOnce([] {}));
  executor.task_runner()->PostDelayedTask(FROM_HERE, BindOnce([] {}),
                                          Milliseconds(10));
  EXPECT_FALSE(CurrentThread::Get()->IsIdleForTesting());
  RunLoop().RunUntilIdle();
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());

  PlatformThread::Sleep(Milliseconds(20));
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
}

TEST_P(SingleThreadTaskExecutorTypedTest, IsIdleForTestingNonNestableTask) {
  SingleThreadTaskExecutor executor(GetParam());
  RunLoop run_loop;
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
  bool nested_task_run = false;
  executor.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] {
        RunLoop nested_run_loop(RunLoop::Type::kNestableTasksAllowed);

        executor.task_runner()->PostNonNestableTask(
            FROM_HERE, BindLambdaForTesting([&] { nested_task_run = true; }));

        executor.task_runner()->PostTask(
            FROM_HERE, BindLambdaForTesting([&] {
              EXPECT_FALSE(nested_task_run);
              EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
            }));

        nested_run_loop.RunUntilIdle();
        EXPECT_FALSE(nested_task_run);
        EXPECT_FALSE(CurrentThread::Get()->IsIdleForTesting());
      }));

  run_loop.RunUntilIdle();

  EXPECT_TRUE(nested_task_run);
  EXPECT_TRUE(CurrentThread::Get()->IsIdleForTesting());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleThreadTaskExecutorTypedTest,
                         ::testing::Values(MessagePumpType::DEFAULT,
                                           MessagePumpType::UI,
                                           MessagePumpType::IO),
                         SingleThreadTaskExecutorTypedTest::ParamInfoToString);

namespace {
// Inject a test point for recording the destructor calls for Closure objects
// send to MessageLoop::PostTask(). It is awkward usage since we are trying to
// hook the actual destruction, which is not a common operation.
class DestructionObserverProbe : public RefCounted<DestructionObserverProbe> {
 public:
  DestructionObserverProbe(bool* task_destroyed,
                           bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {}
  virtual void Run() {
    // This task should never run.
    ADD_FAILURE();
  }

 private:
  friend class RefCounted<DestructionObserverProbe>;

  virtual ~DestructionObserverProbe() {
    EXPECT_FALSE(*destruction_observer_called_);
    *task_destroyed_ = true;
  }

  raw_ptr<bool> task_destroyed_;
  raw_ptr<bool> destruction_observer_called_;
};

class MLDestructionObserver : public CurrentThread::DestructionObserver {
 public:
  MLDestructionObserver(bool* task_destroyed, bool* destruction_observer_called)
      : task_destroyed_(task_destroyed),
        destruction_observer_called_(destruction_observer_called) {}
  void WillDestroyCurrentMessageLoop() override {
    task_destroyed_before_message_loop_ = *task_destroyed_;
    *destruction_observer_called_ = true;
  }
  bool task_destroyed_before_message_loop() const {
    return task_destroyed_before_message_loop_;
  }

 private:
  raw_ptr<bool> task_destroyed_;
  raw_ptr<bool> destruction_observer_called_;
  bool task_destroyed_before_message_loop_ = false;
};

}  // namespace

TEST(SingleThreadTaskExecutorTest, DestructionObserverTest) {
  // Verify that the destruction observer gets called at the very end (after
  // all the pending tasks have been destroyed).
  auto executor = std::make_unique<SingleThreadTaskExecutor>();
  const TimeDelta kDelay = Milliseconds(100);

  bool task_destroyed = false;
  bool destruction_observer_called = false;

  MLDestructionObserver observer(&task_destroyed, &destruction_observer_called);
  CurrentThread::Get()->AddDestructionObserver(&observer);
  executor->task_runner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(&DestructionObserverProbe::Run,
               base::MakeRefCounted<DestructionObserverProbe>(
                   &task_destroyed, &destruction_observer_called)),
      kDelay);
  executor.reset();
  EXPECT_TRUE(observer.task_destroyed_before_message_loop());
  // The task should have been destroyed when we deleted the loop.
  EXPECT_TRUE(task_destroyed);
  EXPECT_TRUE(destruction_observer_called);
}

// Verify that SingleThreadTaskExecutor sets ThreadMainTaskRunner::current() and
// it posts tasks on that message loop.
TEST(SingleThreadTaskExecutorTest, ThreadMainTaskRunner) {
  SingleThreadTaskExecutor executor;
  base::RunLoop loop;
  scoped_refptr<Foo> foo(new Foo());
  std::string a("a");
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(&Foo::Test1ConstRef, foo, a));

  // Post quit task;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindOnce(loop.QuitWhenIdleClosure()));

  // Now kick things off
  loop.Run();

  EXPECT_EQ(foo->test_count(), 1);
  EXPECT_EQ(foo->result(), "a");
}

TEST(SingleThreadTaskExecutorTest, type) {
  SingleThreadTaskExecutor executor(MessagePumpType::UI);
  EXPECT_EQ(executor.type(), MessagePumpType::UI);
}

TEST(SingleThreadTaskExecutorTest,
     ApplicationTasksAllowedInNativeNestedLoopExplicitlyInScope) {
  // Only UI pumps support native loops.
  SingleThreadTaskExecutor executor(MessagePumpType::UI);
  RunLoop run_loop;
  executor.task_runner()->PostTask(
      FROM_HERE,
      BindOnce(
          [](RunLoop* run_loop) {
            {
              CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop
                  allow_nestable_tasks;
              EXPECT_TRUE(CurrentThread::Get()
                              ->ApplicationTasksAllowedInNativeNestedLoop());
            }
            EXPECT_FALSE(CurrentThread::Get()
                             ->ApplicationTasksAllowedInNativeNestedLoop());
            run_loop->Quit();
          },
          Unretained(&run_loop)));
  run_loop.Run();
}

// Verify that tasks posted to and code running in the scope of the same
// SingleThreadTaskExecutor access the same SequenceLocalStorage values.
TEST(SingleThreadTaskExecutorTest, SequenceLocalStorageSetGet) {
  SingleThreadTaskExecutor executor;

  SequenceLocalStorageSlot<int> slot;

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { slot.emplace(11); }));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { EXPECT_EQ(*slot, 11); }));

  RunLoop().RunUntilIdle();
  EXPECT_EQ(*slot, 11);
}

// Verify that tasks posted to and code running in different MessageLoops access
// different SequenceLocalStorage values.
TEST(SingleThreadTaskExecutorTest, SequenceLocalStorageDifferentMessageLoops) {
  SequenceLocalStorageSlot<int> slot;

  {
    SingleThreadTaskExecutor executor;
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindLambdaForTesting([&] { slot.emplace(11); }));

    RunLoop().RunUntilIdle();
    EXPECT_EQ(*slot, 11);
  }

  SingleThreadTaskExecutor executor;
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, BindLambdaForTesting([&] { EXPECT_FALSE(slot); }));

  RunLoop().RunUntilIdle();
  EXPECT_NE(slot.GetOrCreateValue(), 11);
}

namespace {

class PostTaskOnDestroy {
 public:
  explicit PostTaskOnDestroy(int times) : times_remaining_(times) {}

  PostTaskOnDestroy(const PostTaskOnDestroy&) = delete;
  PostTaskOnDestroy& operator=(const PostTaskOnDestroy&) = delete;

  ~PostTaskOnDestroy() { PostTaskWithPostingDestructor(times_remaining_); }

  // Post a task that will repost itself on destruction |times| times.
  static void PostTaskWithPostingDestructor(int times) {
    if (times > 0) {
      SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, DoNothingWithBoundArgs(
                         std::make_unique<PostTaskOnDestroy>(times - 1)));
    }
  }

 private:
  const int times_remaining_;
};

}  // namespace

// Test that SingleThreadTaskExecutor destruction handles a task's destructor
// posting another task.
TEST(SingleThreadTaskExecutorDestructionTest,
     DestroysFineWithPostTaskOnDestroy) {
  SingleThreadTaskExecutor executor;

  PostTaskOnDestroy::PostTaskWithPostingDestructor(10);
}

}  // namespace base
