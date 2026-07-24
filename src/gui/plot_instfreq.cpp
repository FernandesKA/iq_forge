#include "plot_instfreq.h"

#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace iqforge {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

double wrapToPi(double phase) {
  while (phase > kPi) phase -= kTwoPi;
  while (phase < -kPi) phase += kTwoPi;
  return phase;
}
} // namespace

void plotInstFreqLine(const char* plotId, const Sample* data, size_t count, double sampleRateHz,
                      InstFreqViewState& view, bool resetView, SharedXAxisLink& xLink, TimeMarkerState& markers) {
  size_t n = count > 1 ? count - 1 : 0;

  // Reused scratch buffer -- see plot_phase.cpp for why this is safe.
  static std::vector<float> freq;
  freq.resize(n);
  for (size_t i = 0; i < n; ++i) {
    double p0 = std::atan2(data[i].imag(), data[i].real());
    double p1 = std::atan2(data[i + 1].imag(), data[i + 1].real());
    double dPhase = wrapToPi(p1 - p0);
    freq[i] = static_cast<float>(dPhase / kTwoPi * sampleRateHz);
  }

  drawLineView(
      plotId, "Frequency (Hz)", n, resetView, view, xLink, markers,
      [&](double& lo, double& hi) {
        auto [minIt, maxIt] = std::minmax_element(freq.begin(), freq.end());
        lo = *minIt;
        hi = *maxIt;
      },
      [&]() {
        ImPlot::PlotLine("Inst. freq", freq.data(), static_cast<int>(n));

        for (int i = 0; i < kMaxTimeMarkers; ++i) {
          const TimeMarker& m = markers.markers[i];
          if (!m.active || static_cast<size_t>(m.index) >= n) continue;
          const ImVec4& col = timeMarkerColor(i);
          double cx = static_cast<double>(m.index);
          double cy = freq[m.index];
          char id[24];
          std::snprintf(id, sizeof id, "##marker_freq_%d", i);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, col, 1.5f, col);
          ImPlot::PlotScatter(id, &cx, &cy, 1);
          ImPlot::Annotation(cx, cy, col, ImVec2(10, -10), true, "M%d Freq=%.4g Hz", i + 1, cy);
        }
      });
}

} // namespace iqforge
