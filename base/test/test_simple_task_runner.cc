// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_simple_task_runner.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"

namespace base {

TestSimpleTaskRunner::TestSimpleTaskRunner() = default;

TestSimpleTaskRunner::~TestSimpleTaskRunner() = default;

bool TestSimpleTaskRunner::PostDelayedTask(const Location& from_here,
                                           OnceClosure task,
                                           TimeDelta delay) {
  AutoLock auto_lock(lock_);
  pending_tasks_.push_back(TestPendingTask(from_here, std::move(task),
                                           TimeTicks(), delay,
                                           TestPendingTask::NESTABLE));
  return true;
}

bool TestSimpleTaskRunner::PostNonNestableDelayedTask(const Location& from_here,
                                                      OnceClosure task,
                                                      TimeDelta delay) {
  AutoLock auto_lock(lock_);
  pending_tasks_.push_back(TestPendingTask(from_here, std::move(task),
                                           TimeTicks(), delay,
                                           TestPendingTask::NON_NESTABLE));
  return true;
}

// TODO(gab): Use SequenceToken here to differentiate between tasks running in
// the scope of this TestSimpleTaskRunner and other task runners sharing this
// thread. http://crbug.com/631186
bool TestSimpleTaskRunner::RunsTasksInCurrentSequence() const {
  return thread_ref_ == PlatformThread::CurrentRef();
}

std::deque<TestPendingTask> TestSimpleTaskRunner::TakePendingTasks() {
  AutoLock auto_lock(lock_);
  return std::move(pending_tasks_);
}

size_t TestSimpleTaskRunner::NumPendingTasks() const {
  AutoLock auto_lock(lock_);
  return pending_tasks_.size();
}

bool TestSimpleTaskRunner::HasPendingTask() const {
  AutoLock auto_lock(lock_);
  return !pending_tasks_.empty();
}

base::TimeDelta TestSimpleTaskRunner::NextPendingTaskDelay() const {
  AutoLock auto_lock(lock_);
  return pending_tasks_.front().GetTimeToRun() - base::TimeTicks();
}

base::TimeDelta TestSimpleTaskRunner::FinalPendingTaskDelay() const {
  AutoLock auto_lock(lock_);
  return pending_tasks_.back().GetTimeToRun() - base::TimeTicks();
}

void TestSimpleTaskRunner::ClearPendingTasks() {
  AutoLock auto_lock(lock_);
  pending_tasks_.clear();
}

void TestSimpleTaskRunner::RunPendingTasks() {
  DCHECK(RunsTasksInCurrentSequence());

  // Swap with a local variable to avoid re-entrancy problems.
  std::deque<TestPendingTask> tasks_to_run;
  {
    AutoLock auto_lock(lock_);
    tasks_to_run.swap(pending_tasks_);
  }

  // Multiple test task runners can share the same thread for determinism in
  // unit tests. Make sure this TestSimpleTaskRunner's tasks run in its scope.
  ScopedClosureRunner undo_override;
  if (!ThreadTaskRunnerHandle::IsSet() ||
      ThreadTaskRunnerHandle::Get() != this) {
    undo_override = ThreadTaskRunnerHandle::OverrideForTesting(this);
  }

  for (auto& task : tasks_to_run)
    std::move(task.task).Run();
}

void TestSimpleTaskRunner::RunUntilIdle() {
  while (!pending_tasks_.empty()) {
    RunPendingTasks();
  }
}

}  // namespace base
