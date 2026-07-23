#pragma once

#include <deque>
#include <vector>

#include "plot_zoom_controls.h"

namespace iqforge {

// Renders a scrolling spectrogram from `rows` (oldest-to-newest dB rows,
// same convention AppState's waterfall row deques use); newest row on top.
// `zoom` persists pan/zoom state across frames.
void plotWaterfall(const char* plotId, const std::deque<std::vector<float>>& rows, AxisZoomState& zoom);

} // namespace iqforge
