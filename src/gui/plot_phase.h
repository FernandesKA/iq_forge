#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using PhaseViewState = LineViewState;

// Draws the instantaneous phase (atan2(Q, I), wrapped to [-pi, pi]) of an
// already-computed sample window (see plot_trigger.h). `xLink` and `cursor`
// are shared with the I/Q and inst.-freq views over the same window so zoom
// and point-selection stay in sync between them.
void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView,
                    SharedXAxisLink& xLink, SampleCursorState& cursor);

} // namespace iqforge
