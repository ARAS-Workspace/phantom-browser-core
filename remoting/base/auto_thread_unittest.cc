// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/auto_thread.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kThreadName[] = "Test thread";

void SetFlagTask(bool* success) {
  *success = true;
}

void PostSetFlagTask(scoped_refptr<base::TaskRunner> task_runner,
                     bool* success) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(&SetFlagTask, success));
}

}  // namespace

namespace remoting {

class AutoThreadTest : public testing::Test {
 public:
  void RunMessageLoop() {
    // Release |main_task_runner_|, then run |task_environment_| until
    // other references created in tests are gone.  We also post a delayed quit
    // task to |message_loop_| so the test will not hang on failure.
    main_task_runner_.reset();
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(5));
    run_loop.Run();
  }

  void SetUp() override {
    main_task_runner_ = new AutoThreadTaskRunner(
        task_environment_.GetMainThreadTaskRunner(),
        base::BindOnce(&AutoThreadTest::QuitMainMessageLoop,
                       base::Unretained(this)));
  }

  void TearDown() override {
    // Verify that |message_loop_| was quit by the AutoThreadTaskRunner.
    EXPECT_FALSE(quit_closure_);
  }

 protected:
  void QuitMainMessageLoop() { std::move(quit_closure_).Run(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::OnceClosure quit_closure_;
  scoped_refptr<AutoThreadTaskRunner> main_task_runner_;
};

TEST_F(AutoThreadTest, StartAndStop) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner);

  task_runner.reset();
  RunMessageLoop();
}

TEST_F(AutoThreadTest, ProcessTask) {
  // Create an AutoThread joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner);

  // Post a task to it.
  bool success = false;
  task_runner->PostTask(FROM_HERE, base::BindOnce(&SetFlagTask, &success));

  task_runner.reset();
  RunMessageLoop();

  EXPECT_TRUE(success);
}

TEST_F(AutoThreadTest, ThreadDependency) {
  // Create two AutoThreads joined by our MessageLoop.
  scoped_refptr<base::TaskRunner> task_runner1 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner1);
  scoped_refptr<base::TaskRunner> task_runner2 =
      AutoThread::Create(kThreadName, main_task_runner_);
  EXPECT_TRUE(task_runner2);

  // Post a task to thread 1 that will post a task to thread 2.
  bool success = false;
  task_runner1->PostTask(
      FROM_HERE, base::BindOnce(&PostSetFlagTask, task_runner2, &success));

  task_runner1.reset();
  task_runner2.reset();
  RunMessageLoop();

  EXPECT_TRUE(success);
}

}  // namespace remoting
