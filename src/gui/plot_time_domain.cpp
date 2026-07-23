#include "plot_time_domain.h"

#include <implot.h>

#include <algorithm>

namespace iqforge {

void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, SampleCursorState& cursor) {
  drawLineView(
      plotId, "Amplitude", count, resetView, view, xLink, cursor,
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

        if (cursor.active && static_cast<size_t>(cursor.index) < count) {
          double cx = static_cast<double>(cursor.index);
          double ci = data[cursor.index].real();
          double cq = data[cursor.index].imag();
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter("##cursor_i", &cx, &ci, 1);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter("##cursor_q", &cx, &cq, 1);
          ImPlot::Annotation(cx, ci, ImVec4(1, 1, 1, 1), ImVec2(10, -10), true, "I=%.4g", ci);
          ImPlot::Annotation(cx, cq, ImVec4(1, 1, 1, 1), ImVec2(10, 10), true, "Q=%.4g", cq);
        }
      });
}

} // namespace iqforge
