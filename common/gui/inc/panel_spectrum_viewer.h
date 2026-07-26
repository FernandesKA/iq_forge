#pragma once

#include "app_state.h"

namespace iqforge {

// "SpectrumViewer" tab (next to TX/RX): lets the user define one or more
// frequency ranges wider than the device's instantaneous bandwidth, then
// sweep the selected one -- retuning step by step across it and stitching
// each step's spectrum into one wide composite plot.
void drawSpectrumViewerPanel(AppState& state);

} // namespace iqforge
