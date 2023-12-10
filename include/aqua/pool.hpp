#pragma once

#include <atomic>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <vector>

#include "aqua/queue.hpp"

namespace aqua {
/// A thread pool for managing and executing a queue of tasks in parallel.
class thread_pool {
 public:
  /// Constructs a thread pool with a specified number of threads.
  explicit thread_pool(std::size_t thread_count);

  /// Constructs a default thread pool with hardware-concurrency based size.
  explicit thread_pool();

  /// Destructor for cleaning up and joining threads.
  ~thread_pool();

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;

  /// Returns the number of unprocessed tasks in the pool.
  std::size_t size() const;

  /// Submits a task to the pool and returns a future<R> to the caller. All
  /// exceptions are propagated via the returned future.
  template <typename R, typename F, typename... A>
    requires std::is_invocable_r_v<R, F, A...>
  std::future<R> submit(F function, A... arguments) {
    auto shared_promise = std::make_shared<std::promise<R>>();
    std::future<R> future = shared_promise->get_future();

    auto task = [promise = shared_promise, callable = std::move(function),
                 ... args = std::move(arguments)]() {
      try {
        // Call the callable and store the return value in the promise if the
        // callable does not return void
        if constexpr (std::is_same_v<R, void>) {
          callable(args...);
          promise->set_value();
        } else {
          promise->set_value(callable(args...));
        }
      } catch (...) {
        // Propagate the exception if the callable throws an exception
        promise->set_exception(std::current_exception());
      }
    };

    schedule_task(std::move(task));
    return future;
  }

  /// Submits a void-returning task to the pool and returns a future<void> to
  /// the caller. All exceptions are propagated via the returned future.
  template <typename F, typename... A>
    requires std::is_invocable_v<F, A...>
  std::future<void> submit(F function, A... arguments) {
    auto shared_promise = std::make_shared<std::promise<void>>();
    std::future<void> future = shared_promise->get_future();

    auto task = [promise = shared_promise, callable = std::move(function),
                 ... args = std::move(arguments)]() {
      try {
        // Call the callable and initialize the promise with an empty value
        callable(args...);
        promise->set_value();
      } catch (...) {
        // Propagate the exception if the callable throws an exception
        promise->set_exception(std::current_exception());
      }
    };

    schedule_task(std::move(task));
    return future;
  }

 private:
  /// Executes the main loop for each thread, processing tasks from the task
  /// queue until stopped.
  void thread_loop(std::size_t thread_idx);

  /// Schedules a task by adding it to the queue of a selected thread based on a
  /// round robin load balancing policy.
  template <typename F>
  void schedule_task(F&& task) {
    // Find the next thread to push the task onto
    std::size_t next_thread_idx = submitted_tasks % threads.size();
    submitted_tasks.fetch_add(1, std::memory_order_relaxed);
    unprocessed_tasks.fetch_add(1, std::memory_order_relaxed);

    // Push the task to the back of the next thread's task queue
    task_queues[next_thread_idx].tasks.push_back(std::forward<F>(task));

    // Signal the thread that a new task has been added
    task_queues[next_thread_idx].ready.release();
  }

  /// Represents a task queue with thread-safe task storage and a semaphore
  /// indicating task availability.
  struct task_queue {
    aqua::queue<std::function<void()>, std::mutex> tasks;
    std::binary_semaphore ready{0};
  };

  std::atomic_int_fast32_t unprocessed_tasks{};
  std::atomic_int_fast32_t submitted_tasks{};

  std::vector<std::thread> threads;
  std::vector<std::unique_ptr<std::atomic_flag>> stop_flags;
  std::deque<task_queue> task_queues;
};
}  // namespace aqua
