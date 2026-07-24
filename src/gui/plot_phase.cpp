#include "plot_phase.h"

#include <implot.h>

#include <cmath>
#include <vector>

namespace iqforge {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

void plotPhaseLine(const char* plotId, const Sample* data, size_t count, PhaseViewState& view, bool resetView,
                    SharedXAxisLink& xLink, SampleCursorState& cursor) {
  // Reused scratch buffer -- safe across the RX/TX windows sharing this TU
  // because it's filled and fully consumed (by PlotLine, synchronously)
  // within a single call, same convention as plot_waterfall.cpp's `flat`.
  static std::vector<float> phase;
  phase.resize(count);
  for (size_t i = 0; i < count; ++i) {
    phase[i] = std::atan2(data[i].imag(), data[i].real());
  }

  drawLineView(
      plotId, "Phase (rad)", count, resetView, view, xLink, cursor,
      [](double& lo, double& hi) {
        // atan2() always returns a value in [-pi, pi], so the range is
        // fixed rather than scanned from the data.
        lo = -kPi;
        hi = kPi;
      },
      [&]() {
        ImPlot::PlotLine("Phase", phase.data(), static_cast<int>(count));

        for (int slot = 0; slot < SampleCursorState::kCount; ++slot) {
          if (!cursor.active[slot] || static_cast<size_t>(cursor.index[slot]) >= count) continue;
          char tag = slot == 0 ? 'A' : 'B';
          double cx = static_cast<double>(cursor.index[slot]);
          double cy = phase[cursor.index[slot]];
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter(slot == 0 ? "##cursor_phase_a" : "##cursor_phase_b", &cx, &cy, 1);
          ImPlot::Annotation(cx, cy, ImVec4(1, 1, 1, 1), ImVec2(10, -10), true, "%c Phase=%.4g rad", tag, cy);
        }
      });
}

} // namespace iqforge
