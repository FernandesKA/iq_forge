#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"
#include "plot_trigger.h"

namespace iqforge {

using TimeDomainViewState = LineViewState;

// Draws the I/Q line plot for an already-computed sample window (see
// plot_trigger.h for computing that window from a rolling buffer). `xLink`
// and `cursor` are shared with the phase/inst.-freq views over the same
// window so zoom and point-selection stay in sync between them. `trig` (the
// same TriggerState the window was computed from) is used only to draw the
// trigger level as a horizontal line when the trigger is enabled.
void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, SampleCursorState& cursor, const TriggerState& trig);

} // namespace iqforge
