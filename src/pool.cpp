#include <atomic>
#include <iostream>
#include <thread>

#include "aqua/pool.hpp"

aqua::thread_pool::thread_pool()
    : aqua::thread_pool::thread_pool(std::thread::hardware_concurrency()) {}

aqua::thread_pool::thread_pool(const std::size_t thread_count)
    : thread_task_queues(thread_count) {
  std::size_t current_id = 0;

  // Loop to create and start each thread in the pool
  for (std::size_t i = 0; i < thread_count; ++i) {
    priorities.push_back(std::move(current_id));

    try {
      // Create a atomic_flag for each thread to enable cooperative interruption
      interrupt_flags.push_back(std::make_unique<std::atomic_flag>());
      interrupt_flags.back()->clear();

      // Initialize each thread to process tasks from a queue
      threads.emplace_back([&, id = current_id]() {
        do {
          // Block until this thread is signaled to process a task
          thread_task_queues[id].availability.acquire();

          do {
            // Process all available tasks in the queue
            while (auto task = thread_task_queues[id].tasks.front()) {
              thread_task_queues[id].tasks.pop_front();

              try {
                // Decrement the count of unprocessed tasks and execute the task
                unprocessed_tasks.fetch_sub(1, std::memory_order_release);
                std::invoke(std::move(task.value()));
              } catch (...) {
                // Ignore any exceptions thrown by the task
              }

              // Attempt to steal a task from another thread if this thread has
              // no remaining tasks
              for (std::size_t qid = 1; qid < thread_task_queues.size();
                   ++qid) {
                const std::size_t target_id =
                    (id + qid) % thread_task_queues.size();

                if (auto stolen_task =
                        thread_task_queues[target_id].tasks.steal()) {
                  // If a task is successfully stolen, execute it
                  unprocessed_tasks.fetch_sub(1, std::memory_order_release);
                  std::invoke(std::move(stolen_task.value()));

                  // Stop trying to steal more tasks after one is executed
                  break;
                }
              }
            }
          } while (unprocessed_tasks.load(std::memory_order_acquire) > 0);

          // Update the priority of this thread after processing tasks
          priorities.move_front(id);
        } while (!interrupt_flags[id]->test());
      });

      current_id += 1;
    } catch (...) {
      // In case of an exception, remove the last added queue and priority
      thread_task_queues.pop_back();
      priorities.pop_back();
    }
  }
}

aqua::thread_pool::~thread_pool() {
  for (std::size_t i = 0; i < threads.size(); ++i) {
    // Set the atomic flag to true to signal the thread to stop execution
    interrupt_flags[i]->test_and_set();

    thread_task_queues[i].availability.release();

    // Check joinability and then join the thread to avoid dangling threads
    if (threads[i].joinable()) {
      threads[i].join();
    }
  }
}

std::size_t aqua::thread_pool::size() const {
  return static_cast<std::size_t>(unprocessed_tasks);
}
