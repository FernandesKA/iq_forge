#pragma once

#include "app_state.h"

namespace iqforge {

// Draws RX's controls into the current window -- no Begin()/End() of its
// own, since it's meant to be embedded in the merged Control panel
// (panel_control.cpp) rather than owning a "RX Control" window itself.
void drawRxControlContents(AppState& state);

} // namespace iqforge
