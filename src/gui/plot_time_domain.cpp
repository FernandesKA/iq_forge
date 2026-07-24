#include "plot_time_domain.h"

#include <implot.h>

#include <algorithm>
#include <cstdio>

namespace iqforge {

void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, TimeMarkerState& markers, TimeRangeSelection& rangeSel,
                 const TriggerState& trig) {
  drawLineView(
      plotId, "Amplitude", count, resetView, view, xLink, markers, rangeSel,
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

        if (trig.enabled) {
          double lvl = trig.effectiveLevel;
          ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.45f, 0.1f, 0.6f), 1.0f);
          ImPlot::PlotInfLines("##trigger_level", &lvl, 1, ImPlotInfLinesFlags_Horizontal);
        }

        for (int i = 0; i < kMaxTimeMarkers; ++i) {
          const TimeMarker& m = markers.markers[i];
          if (!m.active || static_cast<size_t>(m.index) >= count) continue;
          const ImVec4& col = timeMarkerColor(i);
          double cx = static_cast<double>(m.index);
          double ci = data[m.index].real();
          double cq = data[m.index].imag();
          char idI[16], idQ[16];
          std::snprintf(idI, sizeof idI, "##marker_i_%d", i);
          std::snprintf(idQ, sizeof idQ, "##marker_q_%d", i);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, col, 1.5f, col);
          ImPlot::PlotScatter(idI, &cx, &ci, 1);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, col, 1.5f, col);
          ImPlot::PlotScatter(idQ, &cx, &cq, 1);
          ImPlot::Annotation(cx, ci, col, ImVec2(10, -10), true, "M%d I=%.4g", i + 1, ci);
          ImPlot::Annotation(cx, cq, col, ImVec2(10, 10), true, "M%d Q=%.4g", i + 1, cq);
        }
      });
}

} // namespace iqforge
