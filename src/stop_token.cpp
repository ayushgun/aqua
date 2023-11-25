#include "aqua/stop_token.hpp"
#include <mutex>

void aqua::stop_signal::request_stop() {
  std::scoped_lock lock(mutex);
  stop = true;
}

bool aqua::stop_signal::stop_requested() const {
  std::scoped_lock lock(mutex);
  return stop;
}

aqua::stop_token::stop_token(const aqua::stop_signal& signal)
    : stop_signal(&signal) {}

bool aqua::stop_token::stop_requested() const {
  return stop_signal->stop_requested();
}
