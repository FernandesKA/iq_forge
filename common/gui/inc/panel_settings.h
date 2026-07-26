#pragma once

#include "app_state.h"

namespace iqforge {

// "Settings" window: the auto-save-on-restart toggle (see app_settings.h)
// and named preset save/load/delete.
void drawSettingsPanel(AppState& state);

} // namespace iqforge
