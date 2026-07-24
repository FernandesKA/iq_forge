#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using InstFreqViewState = LineViewState;

// Draws the instantaneous frequency (the phase derivative between
// consecutive samples, converted to Hz via sampleRateHz) of an
// already-computed sample window (see plot_trigger.h). Produces count-1
// points, one per sample-to-sample transition. `xLink` and `markers` are
// shared with the I/Q and phase views over the same window so zoom and
// marker placement stay in sync between them; a marker that lands on the
// last (missing) sample is simply not drawn here.
void plotInstFreqLine(const char* plotId, const Sample* data, size_t count, double sampleRateHz,
                      InstFreqViewState& view, bool resetView, SharedXAxisLink& xLink, TimeMarkerState& markers);

} // namespace iqforge
