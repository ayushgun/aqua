#include <atomic>
#include <functional>
#include <memory>
#include <thread>

#include "aqua/pool.hpp"

aqua::thread_pool::thread_pool()
    : aqua::thread_pool::thread_pool(std::thread::hardware_concurrency()) {}

aqua::thread_pool::thread_pool(const std::size_t thread_count)
    : task_queues(thread_count) {
  // Pre-allocate space for each thread and its stop flag
  threads.reserve(thread_count);
  stop_flags.reserve(thread_count);

  for (std::size_t i = 0; i < thread_count; ++i) {
    // Create a stop flag for this thread to enable cooperative interruption
    stop_flags.push_back(std::make_unique<std::atomic_flag>());
    stop_flags.back()->clear();

    // Initialize the thread with logic to process tasks in the pool
    threads.emplace_back([&, i]() { thread_loop(i); });
  }
}

void aqua::thread_pool::thread_loop(std::size_t thread_idx) {
  while (!stop_flags[thread_idx]->test()) {
    // Acquire the semaphore to block until this thread is signalled to
    // continue processing tasks
    task_queues[thread_idx].ready.acquire();

    // Process tasks while there are still unprocessed tasks left
    while (unprocessed_tasks.load(std::memory_order_acquire) > 0) {
      // Process all available tasks in this thread's task queue
      while (auto task_opt = task_queues[thread_idx].tasks.front()) {
        task_queues[thread_idx].tasks.pop_front();

        // Execute the task and decrement the number of unprocessed tasks
        std::invoke(std::move(*task_opt));
        unprocessed_tasks.fetch_sub(1, std::memory_order_release);
      }
    }
  }
}

aqua::thread_pool::~thread_pool() {
  // Cooperatively interrupt all thread's execution
  for (std::size_t thread_idx = 0; thread_idx < threads.size(); ++thread_idx) {
    stop_flags[thread_idx]->test_and_set();
  }

  // Unblock all threads and join all joinable threads
  for (std::size_t thread_idx = 0; thread_idx < threads.size(); ++thread_idx) {
    task_queues[thread_idx].ready.release();

    if (threads[thread_idx].joinable()) {
      threads[thread_idx].join();
    }
  }
}

std::size_t aqua::thread_pool::size() const {
  return unprocessed_tasks;
}
