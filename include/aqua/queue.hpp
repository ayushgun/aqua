#pragma once

#include <algorithm>
#include <deque>
#include <mutex>
#include <optional>
#include <utility>

namespace aqua {
template <typename T>
concept lockable = requires(T& t) {
  t.lock();
  t.unlock();
  { t.try_lock() } -> std::same_as<bool>;
};

/// Thread-safe queue implementation with lockable policy for synchronization.
template <typename T, lockable L>
class queue {
 public:
  /// Returns the front element of the queue, or nullopt if empty.
  std::optional<T> front() {
    std::scoped_lock lock(mutex);
    return (!backing_queue.empty()) ? std::optional<T>(backing_queue.front())
                                    : std::nullopt;
  }

  /// Returns the back element of the queue, or nullopt if empty.
  std::optional<T> back() {
    std::scoped_lock lock(mutex);
    return (!backing_queue.empty()) ? std::optional<T>(backing_queue.back())
                                    : std::nullopt;
  }

  /// Adds a new element to the back of the queue.
  void push_back(T&& value) {
    std::scoped_lock lock(mutex);
    backing_queue.push_back(std::forward<T>(value));
  }

  /// Adds a new element to the front of the queue.
  void push_front(T&& value) {
    std::scoped_lock lock(mutex);
    backing_queue.push_front(std::forward(value));
  }

  /// Moves an existing element to the front of the queue.
  void move_front(const T& item) {
    std::scoped_lock lock(mutex);
    auto iterator = std::find(backing_queue.begin(), backing_queue.end(), item);

    if (iterator != backing_queue.end()) {
      backing_queue.erase(iterator);
    }

    backing_queue.push_front(item);
  }

  /// Removes the last element from the queue.
  void pop_back() {
    std::scoped_lock lock(mutex);

    if (!backing_queue.empty()) {
      backing_queue.pop_back();
    }
  }

  // Removes the first element from the queue.
  void pop_front() {
    std::scoped_lock lock(mutex);

    if (!backing_queue.empty()) {
      backing_queue.pop_front();
    }
  }

  /// Returns the last element from the queue, or returns nullopt if empty.
  std::optional<T> steal() {
    std::scoped_lock lock(mutex);

    if (backing_queue.empty()) {
      return std::nullopt;
    }

    auto back = std::move(backing_queue.back());
    backing_queue.pop_back();
    return back;
  }

  /// Checks if the queue is empty.
  bool empty() {
    std::scoped_lock lock(mutex);
    return backing_queue.empty();
  }

 private:
  std::deque<T> backing_queue;
  L mutex;
};
}  // namespace aqua
