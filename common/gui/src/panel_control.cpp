#include "panel_control.h"

#include <imgui.h>

#include "dock_tab_utils.h"
#include "panel_rx.h"
#include "panel_signal_viewer.h"
#include "panel_tx.h"

namespace iqforge {

void drawControlPanel(AppState& state) {
  ImGui::Begin("Control");

  // RX and Signal Viewer are checked explicitly; TX is the fallback so it
  // covers both "TX" genuinely being the active tab and the first frame(s)
  // before any dock node exists yet to query (TX is also the default
  // txSourceMode, so defaulting to its controls here matches a fresh app).
  if (isTabActive("RX")) {
    drawRxControlContents(state);
  } else if (isTabActive("Signal Viewer")) {
    drawSignalViewerControlContents(state);
  } else if (isTabActive("SpectrumViewer")) {
    ImGui::TextDisabled("SpectrumViewer's controls live in that window -- nothing to show here.");
  } else {
    drawTxControlContents(state);
  }

  ImGui::End();
}

} // namespace iqforge
