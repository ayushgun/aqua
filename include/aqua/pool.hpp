#pragma once

#include <atomic>
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <semaphore>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "queue.hpp"

namespace aqua {
/// A thread pool for managing and executing a queue of tasks in parallel.
class thread_pool {
 public:
  /// Constructs a thread pool with a specified number of threads.
  explicit thread_pool(const std::size_t thread_count);

  /// Constructs a default thread pool with hardware-concurrency based size.
  explicit thread_pool();

  /// Destructor for cleaning up and joining threads.
  ~thread_pool();

  thread_pool(const thread_pool&) = delete;
  thread_pool& operator=(const thread_pool&) = delete;

  /// Returns the number of threads in the pool.
  std::size_t size() const;

  /// Submits a task to the pool and returns a future<R> to caller.
  template <typename R, typename F, typename... A>
    requires std::is_invocable_r_v<R, F, A...>
  std::future<R> submit(F function, A... arguments) {
    // Create a shared promise to manage the result or exception of the task
    auto shared_promise = std::make_shared<std::promise<R>>();
    auto task_handler = [promise = shared_promise,
                         callable = std::move(function),
                         ... args = std::move(arguments)]() {
      try {
        // Propogate the return type if the callable return type is not void
        if constexpr (std::is_same_v<R, void>) {
          callable(args...);
          promise->set_value();
        } else {
          promise->set_value(callable(args...));
        }
      } catch (const std::exception& exception) {
        // Propogate the return type if the callable throws an exception
        promise->set_exception(std::current_exception());
      }
    };

    auto future = shared_promise->get_future();
    push_task(std::move(task_handler));

    // Return the future to the caller, allowing them to await the task's result
    return future;
  }

 private:
  /// Enqueues a task to the appropriate thread queue based on priority.
  template <typename F>
  void push_task(F&& task_handler) {
    // Get the index of the next thread from the priority queue
    std::optional<std::size_t> next_thread = priorities.front();

    // Exit if the priority queue is empty (i.e., no threads are available)
    if (!next_thread.has_value()) {
      return;
    }

    priorities.pop_front();
    priorities.push_back(std::move(*next_thread));
    unprocessed_tasks.fetch_add(1, std::memory_order_relaxed);

    // Add the task to the selected thread's task queue
    size_t thread_index = *next_thread;
    thread_queues[thread_index].tasks.push_back(std::forward<F>(task_handler));

    // Signal the thread that a new task has been added
    thread_queues[thread_index].availability.release();
  }

  /// Represents a task queue with thread-safe task storage and a semaphore
  /// indicating task availability.
  struct task_queue {
    aqua::queue<std::function<void()>, std::mutex> tasks;
    std::binary_semaphore availability{0};
  };

  std::atomic_bool stop_flag = false;
  std::atomic_int32_t unprocessed_tasks;

  std::vector<std::thread> threads;
  std::deque<task_queue> thread_queues;
  aqua::queue<std::size_t, std::mutex> priorities;
};
}  // namespace aqua
