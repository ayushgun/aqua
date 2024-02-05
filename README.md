# Aqua ![License Badge](https://img.shields.io/badge/license-MIT-blue?link=https%3A%2F%2Fgithub.com%2Fayushgun%2Faqua%2Fblob%2Fmain%2FLICENSE) ![PR Badge](https://img.shields.io/badge/PRs-welcome-red)

Aqua is a modern, minimal, and generic C++ thread pool library designed for managing and executing a queue of tasks concurrently.

## About

Aqua offers a straightforward API to create a pool of worker threads which can execute tasks asynchronously. The library supports custom thread counts or defaults to hardware concurrency. It provides mechanisms for callable submission with future-based result retrieval, ensuring thread safety and concurrent callable execution.

## Key features:

- Customizable thread pool size or default hardware concurrency.
- Task submission with asynchronous result handling using futures.
- Thread-safe task execution with internal task queues and priority handling.
- Clean thread termination and resource cleanup.

## Example Usage

```cpp
#include <iostream>
#include "aqua/pool.hpp"

int main() {
  aqua::thread_pool pool;

  // Submit a callable that increments an integer and returns the result
  auto submission = pool.submit<int>([](int value) { return ++value; }, 1);
  //                        ^^^
  //                        Explicitly provide the return type for non-void callables

  // Unpack the return value propagated to the future if there are no submission errors
  if (!std::holds_alternative<aqua::submission_error>(submission)) {
    auto future = std::get<std::future<int>>(int_result);
    std::cout << "Task returned: " << future.get() << "\n";
  }

  return 0;
}
```
