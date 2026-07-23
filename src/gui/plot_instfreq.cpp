#include "plot_instfreq.h"

#include <implot.h>

#include <algorithm>
#include <cmath>
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
                      InstFreqViewState& view, bool resetView, SharedXAxisLink& xLink, SampleCursorState& cursor) {
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
      plotId, "Frequency (Hz)", n, resetView, view, xLink, cursor,
      [&](double& lo, double& hi) {
        auto [minIt, maxIt] = std::minmax_element(freq.begin(), freq.end());
        lo = *minIt;
        hi = *maxIt;
      },
      [&]() {
        ImPlot::PlotLine("Inst. freq", freq.data(), static_cast<int>(n));

        if (cursor.active && static_cast<size_t>(cursor.index) < n) {
          double cx = static_cast<double>(cursor.index);
          double cy = freq[cursor.index];
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter("##cursor_freq", &cx, &cy, 1);
          ImPlot::Annotation(cx, cy, ImVec4(1, 1, 1, 1), ImVec2(10, -10), true, "Freq=%.4g Hz", cy);
        }
      });
}

} // namespace iqforge
