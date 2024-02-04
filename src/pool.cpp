#include <atomic>
#include <memory>
#include <thread>

#include "aqua/pool.hpp"

aqua::thread_pool::thread_pool()
    : aqua::thread_pool::thread_pool(std::thread::hardware_concurrency()) {}

aqua::thread_pool::thread_pool(const std::size_t thread_count)
    : workers(thread_count) {
  for (std::size_t id = 0; id < thread_count; ++id) {
    // Create a stop flag for this thread to allow for cooperative interruption
    workers[id].stop_flag = std::make_unique<std::atomic_flag>();
    workers[id].stop_flag->clear();

    // Initialize the thread with logic to process tasks in its queue
    workers[id].thread = std::thread([&, id]() { thread_loop(id); });
  }
}

void aqua::thread_pool::thread_loop(std::size_t worker_id) {
  while (!workers[worker_id].stop_flag->test()) {
    // Acquire the semaphore to block until this worker thread is signalled to
    // continue processing tasks
    workers[worker_id].ready.acquire();

    // Process tasks while there are still unprocessed tasks left
    while (unprocessed_tasks.load(std::memory_order_acquire) > 0) {
      while (auto task_opt = workers[worker_id].tasks.front()) {
        workers[worker_id].tasks.pop_front();

        // Execute the task and decrement the number of unprocessed tasks
        std::invoke(std::move(*task_opt));
        unprocessed_tasks.fetch_sub(1, std::memory_order_release);
      }
    }
  }
}

aqua::thread_pool::~thread_pool() {
  // Cooperatively interrupt all worker threads' execution
  for (std::size_t id = 0; id < workers.size(); ++id) {
    workers[id].stop_flag->test_and_set();
  }

  // Unblock all threads and join all threads that can be joined
  for (std::size_t id = 0; id < workers.size(); ++id) {
    workers[id].ready.release();

    if (workers[id].thread.joinable()) {
      workers[id].thread.join();
    }
  }
}

std::size_t aqua::thread_pool::size() const {
  return unprocessed_tasks;
}
