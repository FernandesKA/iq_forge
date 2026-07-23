#include "plot_time_domain.h"

#include <implot.h>

#include <algorithm>

namespace iqforge {

void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView) {
  drawLineView(
      plotId, "Amplitude", count, resetView, view,
      [&](double& lo, double& hi) {
        float flo = data[0].real(), fhi = data[0].real();
        for (size_t i = 0; i < count; ++i) {
          flo = std::min({flo, data[i].real(), data[i].imag()});
          fhi = std::max({fhi, data[i].real(), data[i].imag()});
        }
        lo = flo;
        hi = fhi;
      },
      [&]() {
        const float* base = reinterpret_cast<const float*>(data);
        int n = static_cast<int>(count);
        ImPlot::PlotLine("I", base, n, 1.0, 0.0, 0, 0, sizeof(Sample));
        ImPlot::PlotLine("Q", base + 1, n, 1.0, 0.0, 0, 0, sizeof(Sample));
      });
}

} // namespace iqforge
