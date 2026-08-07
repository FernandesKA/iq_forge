#pragma once

#include "async_iq_load.h"

namespace iqforge {

// Draws a progress bar for an AsyncIqLoadJob that's running(): fill fraction
// is job.progress() (real progress through resampleIq()'s chunked
// conversion, 0 while still reading the file) with "<stage> NN%" as the
// overlay text, so a slow high-ratio resample gives honest, steadily
// advancing feedback instead of an apparently-frozen GUI.
void drawLoadProgressBar(const AsyncIqLoadJob& job);

} // namespace iqforge
