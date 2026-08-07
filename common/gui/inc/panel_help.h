#pragma once

#include "app_state.h"

namespace iqforge {

// Reference documentation window: point-by-point explanations of what each
// panel and control does, organized as collapsible sections so it doesn't
// have to be read top-to-bottom. Static content -- doesn't read or write
// AppState, just takes it for signature consistency with the other
// draw*Panel() functions App::run() calls uniformly.
void drawHelpPanel(AppState& state);

} // namespace iqforge
