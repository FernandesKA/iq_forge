#pragma once

#include <vector>

#include "plot_zoom_controls.h"

namespace iqforge {

struct SpectrumViewState {
  bool hadData = false;
  double sampleRateHz = 0.0;
  AxisZoomState zoom;
};

// Draws one dB-magnitude spectrum plot for `db` (baseband, -sampleRateHz/2
// .. +sampleRateHz/2). `view` persists zoom/fit state across frames and
// automatically re-fits when the sample rate changes.
void plotSpectrum(const char* plotId, const std::vector<float>& db, double sampleRateHz, SpectrumViewState& view);

} // namespace iqforge
