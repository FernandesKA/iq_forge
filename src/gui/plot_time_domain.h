#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using TimeDomainViewState = LineViewState;

// Draws the I/Q line plot for an already-computed sample window (see
// plot_trigger.h for computing that window from a rolling buffer). `xLink`
// and `cursor` are shared with the phase/inst.-freq views over the same
// window so zoom and point-selection stay in sync between them.
void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, SampleCursorState& cursor);

} // namespace iqforge
