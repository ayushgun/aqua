#include <gtest/gtest.h>
#include <thread>
#include "aqua/queue.hpp"

class QueueTest : public ::testing::Test {
 protected:
  aqua::queue<int, std::mutex> test_queue;
};

/// Tests if the front method returns nullopt for an empty queue.
TEST_F(QueueTest, TestEmptyQueueFront) {
  EXPECT_EQ(test_queue.front(), std::nullopt);
}

/// Tests if the back method returns nullopt for an empty queue.
TEST_F(QueueTest, TestEmptyQueueBack) {
  EXPECT_EQ(test_queue.back(), std::nullopt);
}

/// Tests the push_back method by adding an element and verifying it's at the
/// back.
TEST_F(QueueTest, TestPushBack) {
  test_queue.push_back(10);
  EXPECT_EQ(test_queue.back().value(), 10);
}

/// Tests the push_front method by adding an element and verifying it's at the
/// front.
TEST_F(QueueTest, TestPushFront) {
  test_queue.push_front(20);
  EXPECT_EQ(test_queue.front().value(), 20);
}

/// Tests moving an existing element to the front of the queue.
TEST_F(QueueTest, TestMoveFront) {
  test_queue.push_back(30);
  test_queue.push_back(40);
  test_queue.move_front(30);
  EXPECT_EQ(test_queue.front().value(), 30);
}

/// Tests the pop_back method by removing the last element and verifying the
/// change.
TEST_F(QueueTest, TestPopBack) {
  test_queue.push_back(50);
  test_queue.push_back(60);
  test_queue.pop_back();
  EXPECT_EQ(test_queue.back().value(), 50);
}

/// Tests the pop_front method by removing the first element and verifying the
/// change.
TEST_F(QueueTest, TestPopFront) {
  test_queue.push_front(70);
  test_queue.push_front(80);
  test_queue.pop_front();
  EXPECT_EQ(test_queue.front().value(), 70);
}

/// Tests the steal method to ensure it returns and removes the last element.
TEST_F(QueueTest, TestSteal) {
  test_queue.push_back(90);
  auto stolen = test_queue.steal();
  EXPECT_EQ(stolen.value(), 90);
  EXPECT_TRUE(test_queue.empty());
}

/// Tests the empty method for both empty and non-empty states.
TEST_F(QueueTest, TestEmpty) {
  EXPECT_TRUE(test_queue.empty());
  test_queue.push_back(100);
  EXPECT_FALSE(test_queue.empty());
}

/// Tests the thread safety of the push_back method in a multi-threaded context.
TEST_F(QueueTest, TestThreadSafetyPushBack) {
  std::vector<std::thread> threads;
  for (std::size_t i = 0; i < 100; ++i) {
    threads.emplace_back(
        [this, i] { test_queue.push_back(static_cast<int>(i)); });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_FALSE(test_queue.empty());
}
