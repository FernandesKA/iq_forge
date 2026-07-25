#pragma once

#include <cstddef>

#include "sample_types.h"
#include "plot_line_view.h"
#include "plot_trigger.h"

namespace iqforge {

using TimeDomainViewState = LineViewState;

// Draws the I/Q line plot for an already-computed sample window (see
// plot_trigger.h for computing that window from a rolling buffer). `xLink`,
// `markers` and `rangeSel` are shared with the phase/inst.-freq views over
// the same window so zoom, marker placement and range selection stay in
// sync between them. `trig` (the same TriggerState the window was computed
// from) is used only to draw the trigger level as a horizontal line when
// the trigger is enabled.
void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, TimeMarkerState& markers, TimeRangeSelection& rangeSel,
                 const TriggerState& trig);

} // namespace iqforge
