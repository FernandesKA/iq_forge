#pragma once

#include "app_state.h"

namespace iqforge {

// Consolidated per-direction visualization windows: Spectrum, Waterfall,
// I/Q, Phase, and Instantaneous Frequency stacked vertically, each
// independently toggleable via a checkbox. TX shows what's being
// transmitted (generator/file preview); RX shows what's being received.
void drawTxVisualizationPanel(AppState& state);
void drawRxVisualizationPanel(AppState& state);

} // namespace iqforge
