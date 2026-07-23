#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using PhaseViewState = LineViewState;

// Draws the instantaneous phase (atan2(Q, I), wrapped to [-pi, pi]) of an
// already-computed sample window (see plot_trigger.h).
void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView);

} // namespace iqforge
