#pragma once

#include "app_state.h"

namespace iqforge {

// Draws TX's controls into the current window -- no Begin()/End() of its
// own, since it's meant to be embedded in the merged Control panel
// (panel_control.cpp) rather than owning a "TX Control" window itself.
void drawTxControlContents(AppState& state);

} // namespace iqforge
