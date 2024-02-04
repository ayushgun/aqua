#pragma once

#include <atomic>
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
/// A thread pool for managing and executing a queue of worker tasks
/// concurrently.
class thread_pool {
 public:
  /// Constructs a thread pool with a specified number of worker threads.
  explicit thread_pool(std::size_t thread_count);

  /// Constructs a default thread pool with a default number of worker threads.
  explicit thread_pool();

  /// Destructor for cleaning up and joining worker threads.
  ~thread_pool();

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;

  /// Returns the number of unprocessed tasks in the pool.
  std::size_t size() const;

  /// Submits a task to the pool and returns a future<R> to the caller. All
  /// return values or exceptions are propagated via the returned future.
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
  /// the caller. All  return values or exceptions are propagated via the
  /// returned future.
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
  /// Executes the main loop for each worker thread, processing tasks from the
  /// task queue until stopped.
  void thread_loop(std::size_t worker_id);

  /// Schedules a task by adding it to the queue of a worker thread. Uses the
  /// round robin scheduling algorithm.
  template <typename F>
  void schedule_task(F&& task) {
    // Find the worker thread to assign the task to
    current_task_id.fetch_add(1, std::memory_order_relaxed);
    unprocessed_tasks.fetch_add(1, std::memory_order_relaxed);
    std::size_t worker_id = current_task_id % workers.size();

    // Push the task to the back of the scheduled worker's task queue
    workers[worker_id].tasks.push_back(std::forward<F>(task));

    // Signal the worker that tasks are ready to be completed
    workers[worker_id].ready.release();
  }

  // Represents a worker in a thread pool. Manages task execution,
  // synchronization, and lifecycle control.
  struct worker {
    std::binary_semaphore ready{0};
    std::unique_ptr<std::atomic_flag> stop_flag;
    aqua::queue<std::function<void()>, std::mutex> tasks;
    std::thread thread;
  };

  std::atomic_int_fast16_t current_task_id;
  std::atomic_int_fast32_t unprocessed_tasks;
  std::vector<worker> workers;
};
}  // namespace aqua
