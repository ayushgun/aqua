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
    threads.emplace_back([&, thread_id = i]() {
      while (!stop_flags[thread_id]->test()) {
        // Acquire the semaphore to block until this thread is signalled to
        // continue processing tasks
        task_queues[thread_id].ready.acquire();

        // Process tasks while there are still unprocessed tasks left
        while (unprocessed_tasks.load(std::memory_order_acquire) > 0) {
          // Process all available tasks in this thread's task queue
          while (auto task = task_queues[thread_id].tasks.front()) {
            task_queues[thread_id].tasks.pop_front();

            // Execute the task and decrement the number of unprocessed tasks
            std::invoke(std::move(*task));
            unprocessed_tasks.fetch_sub(1, std::memory_order_release);
          }

          // Attempt to steal a task if this thread has no remaining tasks
          for (std::size_t offset = 0; offset < threads.size(); ++offset) {
            std::size_t target_thread_id =
                (thread_id + offset) % threads.size();

            // Steal the next queued up task from this thread's task queue
            if (auto stolen_task = task_queues[target_thread_id].tasks.back()) {
              // Execute the task and decrement the number of unprocessed tasks
              std::invoke(std::move(*stolen_task));
              unprocessed_tasks.fetch_sub(1, std::memory_order_release);

              // Stop trying to steal more tasks after one is executed
              break;
            }
          }
        }
      }
    });
  }
}

aqua::thread_pool::~thread_pool() {
  // Cooperatively interrupt all thread's execution
  for (std::size_t thread_id = 0; thread_id < threads.size(); ++thread_id) {
    stop_flags[thread_id]->test_and_set();
  }

  // Unblock all threads and join all joinable threads
  for (std::size_t thread_id = 0; thread_id < threads.size(); ++thread_id) {
    task_queues[thread_id].ready.release();

    if (threads[thread_id].joinable()) {
      threads[thread_id].join();
    }
  }
}

std::size_t aqua::thread_pool::size() const {
  return unprocessed_tasks;
}
