#include <gtest/gtest.h>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include "aqua/pool.hpp"

/// Calculate the nth Fibonacci number recursively.
int fib(int n) {
  return (n <= 1) ? n : fib(n - 1) + fib(n - 2);
}

/// Perform a computational task that calculates the sum of Fibonacci numbers up
/// to 'n'. This task is designed to simulate CPU-bound work.
std::pair<std::thread::id, int> task(int n) {
  int r = 0;
  for (int i = 0; i <= n; ++i) {
    r += fib(i);
    // Introduce a delay every 5 iterations to simulate varied task durations
    if (i % 5 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
  return {std::this_thread::get_id(), r};
}

/// Test fixture for thread pool tests, providing a thread pool instance for
/// each test.
class ThreadPoolTest : public ::testing::Test {
 protected:
  aqua::thread_pool test_pool;
};

/// Test to check if tasks are executed and produce the correct number of
/// results when submitted from two distinct threads.
TEST_F(ThreadPoolTest, TaskExecutionAndResult) {
  std::mutex futures_mutex;
  std::vector<std::future<std::pair<std::thread::id, int>>> futures;

  // Lambda to simulate submitting tasks to the thread pool
  auto submit_tasks =
      [&](aqua::thread_pool& pool,
          std::vector<std::future<std::pair<std::thread::id, int>>>& futures,
          int count) {
        for (int i = 0; i < count; ++i) {
          auto fut = pool.submit<std::pair<std::thread::id, int>>(task, i);

          // Lock to synchronize access to futures
          std::lock_guard<std::mutex> lock(futures_mutex);
          futures.push_back(std::move(fut));
        }
      };

  // Submit tasks in parallel threads
  std::thread thread1(submit_tasks, std::ref(test_pool), std::ref(futures), 5);
  std::thread thread2(submit_tasks, std::ref(test_pool), std::ref(futures), 5);
  thread1.join();
  thread2.join();

  EXPECT_EQ(futures.size(), 10);
}

/// Test to verify if tasks are evenly distributed across different threads.
TEST_F(ThreadPoolTest, TaskDistribution) {
  std::vector<std::future<std::pair<std::thread::id, int>>> futures;
  std::unordered_map<std::thread::id, int> thread_distribution;

  // Submit a number of tasks equal to three times the hardware concurrency
  std::size_t total_tasks = std::thread::hardware_concurrency() * 3;
  for (std::size_t i = 0; i < total_tasks; ++i) {
    futures.push_back(
        test_pool.submit<std::pair<std::thread::id, int>>(task, i));
  }

  // Process the results and count tasks per thread
  for (auto& future : futures) {
    auto result = future.get();
    thread_distribution[result.first] += 1;
  }

  // Check if the number of threads used equals the thread count of the pool
  EXPECT_EQ(thread_distribution.size(), std::thread::hardware_concurrency());
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
