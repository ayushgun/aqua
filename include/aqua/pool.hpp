#pragma once

#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <variant>
#include <vector>

#include <boost/lockfree/spsc_queue.hpp>

namespace aqua {
enum class submission_error {
  queue_full,
};

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
  std::variant<std::future<R>, submission_error> submit(F function,
                                                        A... arguments) {
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

    // If scheduling fails, return the encountered submission error
    std::optional<submission_error> error_opt = schedule_task(std::move(task));
    if (error_opt) {
      return error_opt.value();
    }

    // Otherwise, return the future associated with the task's execution
    return future;
  }

  /// Submits a void-returning task to the pool and returns a future<void> to
  /// the caller. All  return values or exceptions are propagated via the
  /// returned future.
  template <typename F, typename... A>
    requires std::is_invocable_v<F, A...>
  std::variant<std::future<void>, submission_error> submit(F function,
                                                           A... arguments) {
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

    // If scheduling fails, return the encountered submission error
    std::optional<submission_error> error_opt = schedule_task(std::move(task));
    if (error_opt) {
      return error_opt.value();
    }

    // Otherwise, return the future associated with the task's execution
    return future;
  }

 private:
  /// Executes the main loop for each worker thread, processing tasks from the
  /// task queue until stopped.
  void thread_loop(std::size_t worker_id);

  /// Schedules a task by adding it to the queue of a worker thread. Uses the
  /// round robin scheduling algorithm.
  template <typename F>
  std::optional<submission_error> schedule_task(F&& task) {
    unprocessed_tasks.fetch_add(1, std::memory_order_relaxed);

    // Find the worker thread to assign the task to
    next_worker_id.fetch_add(1, std::memory_order_relaxed);
    next_worker_id = next_worker_id % workers.size();

    // Attempt to push the task to the back of the scheduled worker's task queue
    // if it is not full
    if (!workers[next_worker_id].tasks.push(std::forward<F>(task))) {
      return submission_error::queue_full;
    }

    // Signal the worker that tasks are ready to be completed
    workers[next_worker_id].ready.release();
    return std::nullopt;
  }

  // Represents a worker in a thread pool. Manages task execution,
  // synchronization, and lifecycle control.
  struct worker {
    std::binary_semaphore ready{0};
    std::unique_ptr<std::atomic_flag> stop_flag;
    boost::lockfree::spsc_queue<std::function<void()>> tasks{1024};
    std::thread thread;
  };

  std::atomic_int16_t next_worker_id;
  std::atomic_int32_t unprocessed_tasks;
  std::vector<worker> workers;
};
}  // namespace aqua
