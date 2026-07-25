#pragma once

#include "sample_types.h"

namespace iqforge {

// Anything that can produce a stream of IQ samples on demand: a signal
// generator, a looping/one-shot file player, etc. Devices pull from this on
// their own TX thread.
class ISampleSource {
 public:
  virtual ~ISampleSource() = default;

  // Fill up to `count` samples into `out`. Returns the number actually
  // written. A return value of 0 means the source is exhausted (e.g. a
  // non-looping file that reached EOF); callers should stop TX in that case.
  virtual size_t generate(Sample* out, size_t count) = 0;
};

} // namespace iqforge
