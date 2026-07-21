#include <thread>

#include "../src/core/ring_buffer.h"
#include "test_framework.h"

using iqforge::RingBuffer;

void run_ring_buffer_tests() {
  {
    RingBuffer<int> rb(4);
    rb.push(1);
    rb.push(2);
    rb.push(3);
    int out = 0;
    CHECK(rb.pop(out) && out == 1);
    CHECK(rb.pop(out) && out == 2);
    CHECK(rb.pop(out) && out == 3);
    CHECK(!rb.tryPop(out)); // empty
  }

  {
    // pushLatest must drop the oldest entry once at capacity.
    RingBuffer<int> rb(2);
    rb.pushLatest(1);
    rb.pushLatest(2);
    rb.pushLatest(3); // should evict 1
    int out = 0;
    CHECK(rb.tryPop(out) && out == 2);
    CHECK(rb.tryPop(out) && out == 3);
    CHECK(!rb.tryPop(out));
  }

  {
    // blocking pop() must unblock and return false once stop() is called.
    RingBuffer<int> rb(1);
    std::thread t([&rb] { rb.stop(); });
    int out = 0;
    bool got = rb.pop(out);
    t.join();
    CHECK(!got);
  }

  {
    // push()/pop() hand data across threads correctly.
    RingBuffer<int> rb(2);
    std::thread producer([&rb] {
      for (int i = 0; i < 100; ++i) rb.push(i);
    });
    int sum = 0;
    for (int i = 0; i < 100; ++i) {
      int v = 0;
      rb.pop(v);
      sum += v;
    }
    producer.join();
    CHECK(sum == 99 * 100 / 2);
  }
}
