#pragma once

#include <deque>

#include "plot_zoom_controls.h"
#include "waterfall_row.h"

namespace iqforge {

// Renders a scrolling spectrogram from `rows` (oldest-to-newest dB rows,
// same convention AppState's waterfall row deques use); newest row on top.
// The Y axis is labeled with how long ago each row was captured, and a
// "last row: Ns ago" readout above the plot makes it obvious whether the
// waterfall is still advancing. `zoom` persists pan/zoom state across
// frames.
void plotWaterfall(const char* plotId, const std::deque<WaterfallRow>& rows, AxisZoomState& zoom);

} // namespace iqforge
