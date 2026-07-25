#pragma once

#include <cstddef>

#include "sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using PhaseViewState = LineViewState;

// Draws the instantaneous phase (atan2(Q, I), wrapped to [-pi, pi]) of an
// already-computed sample window (see plot_trigger.h). `xLink`, `markers`
// and `rangeSel` are shared with the I/Q and inst.-freq views over the same
// window so zoom, marker placement and range selection stay in sync between
// them.
void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView,
                    SharedXAxisLink& xLink, TimeMarkerState& markers, TimeRangeSelection& rangeSel);

} // namespace iqforge
