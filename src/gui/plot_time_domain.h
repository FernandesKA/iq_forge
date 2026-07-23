#pragma once

#include <cstddef>

#include "../core/sample_types.h"
#include "plot_line_view.h"

namespace iqforge {

using TimeDomainViewState = LineViewState;

// Draws the I/Q line plot for an already-computed sample window (see
// plot_trigger.h for computing that window from a rolling buffer).
void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView);

} // namespace iqforge
