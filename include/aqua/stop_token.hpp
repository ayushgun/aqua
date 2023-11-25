#include <mutex>

namespace aqua {
/// Represents a signal for cooperatively interrupting threads. Maintains a stop
/// state that can be set and queried in a thread-safe manner.
class stop_signal {
 public:
  /// Signals to request the stopping of the associated process.
  void request_stop();

  /// Checks if a stop has been requested.
  bool stop_requested() const;

 private:
  mutable std::mutex mutex;
  bool stop = false;
};

/// Token that can be used to check if a stop has been requested.
class stop_token {
 public:
  /// Constructs a stop_token with a reference to a stop_signal.
  explicit stop_token(const aqua::stop_signal& signal);

  /// Checks if the associated stop_signal has requested a stop.
  bool stop_requested() const;

 private:
  const aqua::stop_signal* stop_signal;
};
}  // namespace aqua
