#pragma once

#include <deque>

#include "plot_zoom_controls.h"
#include "waterfall_row.h"

namespace iqforge {

// Color range and appearance controls for the waterfall, plus zoom/pan --
// persists across frames the same way SpectrumViewState does for the
// spectrum plot.
struct WaterfallViewState {
  AxisZoomState zoom;
  bool gridEnabled = false;
  int colormapIndex = 0; // index into kWaterfallColormaps, see plot_waterfall.cpp
  float colorMinDb = -100.0f;
  float colorMaxDb = 0.0f;
};

// Renders a scrolling spectrogram from `rows` (oldest-to-newest dB rows,
// same convention AppState's waterfall row deques use); newest row on top.
// The Y axis is labeled with how long ago each row was captured, and a
// "last row: Ns ago" readout above the plot makes it obvious whether the
// waterfall is still advancing. A measurements/controls panel to the right
// (matching the spectrum plot's, so both plots stay the same width -- see
// kSpectrumMeasurementsWidth) shows the latest row's peak and lets the
// color range, colormap, and grid be adjusted. `sampleRateHz` is only used
// to convert the latest row's peak bin to a frequency for that readout.
// `view` persists zoom/pan/appearance state across frames.
void plotWaterfall(const char* plotId, const std::deque<WaterfallRow>& rows, double sampleRateHz,
                    WaterfallViewState& view);

} // namespace iqforge
