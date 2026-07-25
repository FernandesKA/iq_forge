#pragma once

#include <cstdio>
#include <string>

namespace iqforge {

// Human-readable frequency for marker/measurement readouts (display only --
// FrequencyInputHz in freq_input.h is the editable-value counterpart).
inline std::string formatHz(double hz) {
  double a = hz < 0 ? -hz : hz;
  char buf[64];
  if (a >= 1e9) {
    std::snprintf(buf, sizeof buf, "%.4f GHz", hz / 1e9);
  } else if (a >= 1e6) {
    std::snprintf(buf, sizeof buf, "%.4f MHz", hz / 1e6);
  } else if (a >= 1e3) {
    std::snprintf(buf, sizeof buf, "%.4f kHz", hz / 1e3);
  } else {
    std::snprintf(buf, sizeof buf, "%.1f Hz", hz);
  }
  return buf;
}

// Human-readable time span for cursor Delta-t readouts.
inline std::string formatSeconds(double s) {
  double a = s < 0 ? -s : s;
  char buf[64];
  if (a >= 1.0) {
    std::snprintf(buf, sizeof buf, "%.6f s", s);
  } else if (a >= 1e-3) {
    std::snprintf(buf, sizeof buf, "%.4f ms", s * 1e3);
  } else if (a >= 1e-6) {
    std::snprintf(buf, sizeof buf, "%.4f us", s * 1e6);
  } else {
    std::snprintf(buf, sizeof buf, "%.4f ns", s * 1e9);
  }
  return buf;
}

} // namespace iqforge
