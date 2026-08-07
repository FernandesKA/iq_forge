#pragma once

#include "app_state.h"

namespace iqforge {

// Consolidated per-direction visualization windows: Spectrum, Waterfall,
// I/Q, Phase, and Instantaneous Frequency stacked vertically, each
// independently toggleable via a checkbox. TX shows what's being
// transmitted (generator/file preview); RX shows what's being received.
void drawTxVisualizationPanel(AppState& state);
void drawRxVisualizationPanel(AppState& state);
// Signal Viewer shows whatever file panel_signal_viewer.cpp currently has
// loaded -- no device/TX/RX involved, so its "-> Center freq" marker action
// just updates the display reference (state.svCenterFreqHz) instead of
// retuning any hardware.
void drawSignalViewerVisualizationPanel(AppState& state);

} // namespace iqforge
