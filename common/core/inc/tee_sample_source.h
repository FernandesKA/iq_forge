#pragma once

#include <memory>

#include "ring_buffer.h"
#include "sample_source.h"

namespace iqforge {

// Wraps another ISampleSource and mirrors every generated block into a
// RingBuffer, so the GUI can preview exactly what's being transmitted
// without racing the real TX thread by calling generate() a second time
// (which would desync stateful generators like SignalGenerator).
class TeeSampleSource : public ISampleSource {
 public:
  TeeSampleSource(std::shared_ptr<ISampleSource> inner, RingBuffer<SampleBuffer>* previewRing)
      : inner_(std::move(inner)), previewRing_(previewRing) {}

  size_t generate(Sample* out, size_t count) override {
    size_t n = inner_->generate(out, count);
    if (n > 0 && previewRing_) {
      previewRing_->pushLatest(SampleBuffer(out, out + n));
    }
    return n;
  }

 private:
  std::shared_ptr<ISampleSource> inner_;
  RingBuffer<SampleBuffer>* previewRing_;
};

} // namespace iqforge
