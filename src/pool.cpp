#include <atomic>
#include <exception>
#include <functional>
#include <thread>

#include "aqua/pool.hpp"

aqua::thread_pool::thread_pool()
    : aqua::thread_pool::thread_pool(std::thread::hardware_concurrency()) {}

aqua::thread_pool::thread_pool(const std::size_t thread_count)
    : thread_queues(thread_count) {
  std::size_t current_id = 0;

  // Initialize the specified number of threads
  for (std::size_t i = 0; i < thread_count; ++i) {
    priorities.push_back(std::move(current_id));

    try {
      // Create and start new thread with a lambda function as the thread's task
      threads.emplace_back([&, id = current_id]() {
        while (!stop_flag.load()) {
          // Block until this thread is signaled to process a task
          thread_queues[id].availability.acquire();

          // Check for pending tasks and execute them
          while (unprocessed_tasks.load(std::memory_order_acquire) > 0) {
            // Process tasks assigned to this thread
            while (auto task = thread_queues[id].tasks.front()) {
              thread_queues[id].tasks.pop_front();

              // Decrease count of pending tasks and complete each task
              unprocessed_tasks.fetch_sub(1, std::memory_order_release);
              std::invoke(std::move(task.value()));
            }

            // Attempt to steal a task from another thread if this thread has no
            // remaining tasks
            for (std::size_t qid = 1; qid < thread_queues.size(); ++qid) {
              const std::size_t target_id = (id + qid) % thread_queues.size();

              if (auto stolen_task = thread_queues[target_id].tasks.steal()) {
                // If a task is successfully stolen, execute it
                unprocessed_tasks.fetch_sub(1, std::memory_order_release);
                std::invoke(std::move(stolen_task.value()));

                // Stop trying to steal more tasks after one is executed
                break;
              }
            }
          }

          // Once tasks are completed, assign this thread the highest priority
          priorities.move_front(id);
        }
      });

      current_id += 1;
    } catch (const std::exception& exception) {
      // Remove exception origin thread from the task queue and priority queue
      thread_queues.pop_back();
      priorities.pop_back();
    }
  }
}

aqua::thread_pool::~thread_pool() {
  stop_flag.store(true);

  // Stop and join all threads in the pool
  for (std::size_t i = 0; i < threads.size(); ++i) {
    thread_queues[i].availability.release();
    threads[i].join();
  }
}

size_t aqua::thread_pool::size() const {
  return threads.size();
}
