#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using InstFreqViewState = LineViewState;

// Draws the instantaneous frequency (the phase derivative between
// consecutive samples, converted to Hz via sampleRateHz) of an
// already-computed sample window (see plot_trigger.h). Produces count-1
// points, one per sample-to-sample transition.
void plotInstFreqLine(const char* plotId, const Sample* data, size_t count, double sampleRateHz,
                      InstFreqViewState& view, bool resetView);

} // namespace iqforge
