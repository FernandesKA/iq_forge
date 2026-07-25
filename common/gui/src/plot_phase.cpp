#include "plot_phase.h"

#include <implot.h>

#include <cmath>
#include <cstdio>
#include <vector>

namespace iqforge {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView,
                    SharedXAxisLink& xLink, TimeMarkerState& markers, TimeRangeSelection& rangeSel) {
  // Reused scratch buffer -- safe across the RX/TX windows sharing this TU
  // because it's filled and fully consumed (by PlotLine, synchronously)
  // within a single call, same convention as plot_waterfall.cpp's `flat`.
  static std::vector<float> phase;
  phase.resize(count);
  for (size_t i = 0; i < count; ++i) {
    phase[i] = std::atan2(data[i].imag(), data[i].real());
  }

  drawLineView(
      plotId, "Phase (rad)", count, resetView, view, xLink, markers, rangeSel,
      [](double& lo, double& hi) {
        // atan2() always returns a value in [-pi, pi], so the range is
        // fixed rather than scanned from the data.
        lo = -kPi;
        hi = kPi;
      },
      [&]() {
        ImPlot::PlotLine("Phase", phase.data(), static_cast<int>(count));

        for (int i = 0; i < kMaxTimeMarkers; ++i) {
          const TimeMarker& m = markers.markers[i];
          if (!m.active || static_cast<size_t>(m.index) >= count) continue;
          const ImVec4& col = timeMarkerColor(i);
          double cx = static_cast<double>(m.index);
          double cy = phase[m.index];
          char id[24];
          std::snprintf(id, sizeof id, "##marker_phase_%d", i);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, col, 1.5f, col);
          ImPlot::PlotScatter(id, &cx, &cy, 1);
          ImPlot::Annotation(cx, cy, col, ImVec2(10, -10), true, "M%d Phase=%.4g rad", i + 1, cy);
        }
      });
}

} // namespace iqforge
