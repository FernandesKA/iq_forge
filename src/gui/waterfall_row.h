#pragma once

#include <chrono>
#include <vector>

namespace iqforge {

// One spectrogram row: a dB-per-bin spectrum plus the time it was captured.
// The timestamp lets the waterfall plot show how far back its history
// extends and, more importantly, how long ago the newest row arrived --
// otherwise there's no way to tell whether it's still advancing.
struct WaterfallRow {
  std::vector<float> db;
  std::chrono::steady_clock::time_point timestamp;
};

} // namespace iqforge
