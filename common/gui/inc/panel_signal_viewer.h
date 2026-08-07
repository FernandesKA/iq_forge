#pragma once

#include "app_state.h"

namespace iqforge {

// Draws Signal Viewer's controls into the current window -- no Begin()/
// End() of its own, since it's meant to be embedded in the merged Control
// panel (panel_control.cpp) rather than owning a "Signal Viewer Control"
// window itself.
void drawSignalViewerControlContents(AppState& state);

} // namespace iqforge
