#include "plot_phase.h"

#include <implot.h>

#include <cmath>
#include <vector>

namespace iqforge {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView) {
  // Reused scratch buffer -- safe across the RX/TX windows sharing this TU
  // because it's filled and fully consumed (by PlotLine, synchronously)
  // within a single call, same convention as plot_waterfall.cpp's `flat`.
  static std::vector<float> phase;
  phase.resize(count);
  for (size_t i = 0; i < count; ++i) {
    phase[i] = std::atan2(data[i].imag(), data[i].real());
  }

  drawLineView(
      plotId, "Phase (rad)", count, resetView, view,
      [](double& lo, double& hi) {
        // atan2() always returns a value in [-pi, pi], so the range is
        // fixed rather than scanned from the data.
        lo = -kPi;
        hi = kPi;
      },
      [&]() { ImPlot::PlotLine("Phase", phase.data(), static_cast<int>(count)); });
}

} // namespace iqforge
