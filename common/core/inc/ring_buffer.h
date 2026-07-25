#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>

namespace iqforge {

// Bounded thread-safe queue of blocks (T is typically SampleBuffer). Used to
// hand streams of IQ blocks between a device I/O thread and the GUI/DSP
// consumer without either side touching the other's internals.
//
// Two push/pop styles are supported because producer and consumer have
// different real-time requirements:
//  - push()/pop(): blocking, used on the hot device<->generator path where
//    we want backpressure rather than dropped samples.
//  - pushLatest()/tryPop(): non-blocking; pushLatest() drops the oldest
//    block when full, tryPop() returns immediately. Used for RX display
//    buffers where the GUI only cares about recent data and must never
//    stall the capture thread.
template <typename T>
class RingBuffer {
 public:
  explicit RingBuffer(size_t capacity) : capacity_(capacity) {}

  void push(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    notFull_.wait(lock, [this] { return stopped_ || queue_.size() < capacity_; });
    if (stopped_) return;
    queue_.push_back(std::move(item));
    lock.unlock();
    notEmpty_.notify_one();
  }

  bool pop(T& out) {
    std::unique_lock<std::mutex> lock(mutex_);
    notEmpty_.wait(lock, [this] { return stopped_ || !queue_.empty(); });
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    lock.unlock();
    notFull_.notify_one();
    return true;
  }

  void pushLatest(T item) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.size() >= capacity_) {
      queue_.pop_front();
    }
    queue_.push_back(std::move(item));
    lock.unlock();
    notEmpty_.notify_one();
  }

  bool tryPop(T& out) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (queue_.empty()) return false;
    out = std::move(queue_.front());
    queue_.pop_front();
    return true;
  }

  // Wakes any blocked push()/pop() callers so threads can shut down cleanly.
  void stop() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = true;
    lock.unlock();
    notEmpty_.notify_all();
    notFull_.notify_all();
  }

  void reset() {
    std::unique_lock<std::mutex> lock(mutex_);
    stopped_ = false;
    queue_.clear();
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;
  std::deque<T> queue_;
  size_t capacity_;
  bool stopped_ = false;
};

} // namespace iqforge
