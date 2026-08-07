#pragma once

#include "app_state.h"

namespace iqforge {

// Single "Control" window whose content follows whichever main-area window
// (TX/RX/Signal Viewer) is currently the selected tab -- rather than
// separately-tabbed TX Control/RX Control/Signal Viewer Control windows the
// user has to click through independently of the main-area tabs.
void drawControlPanel(AppState& state);

} // namespace iqforge
