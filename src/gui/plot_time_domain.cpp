#include "plot_time_domain.h"

#include <implot.h>

#include <algorithm>

namespace iqforge {

void plotIQLines(const char* plotId, const Sample* data, size_t count, TimeDomainViewState& view, bool resetView,
                 SharedXAxisLink& xLink, SampleCursorState& cursor, const TriggerState& trig) {
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

        if (trig.enabled) {
          double lvl = trig.effectiveLevel;
          ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.45f, 0.1f, 0.6f), 1.0f);
          ImPlot::PlotInfLines("##trigger_level", &lvl, 1, ImPlotInfLinesFlags_Horizontal);
        }

        for (int slot = 0; slot < SampleCursorState::kCount; ++slot) {
          if (!cursor.active[slot] || static_cast<size_t>(cursor.index[slot]) >= count) continue;
          char tag = slot == 0 ? 'A' : 'B';
          double cx = static_cast<double>(cursor.index[slot]);
          double ci = data[cursor.index[slot]].real();
          double cq = data[cursor.index[slot]].imag();
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter(slot == 0 ? "##cursor_i_a" : "##cursor_i_b", &cx, &ci, 1);
          ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 6, ImVec4(1, 1, 1, 1), 1.5f, ImVec4(1, 1, 1, 1));
          ImPlot::PlotScatter(slot == 0 ? "##cursor_q_a" : "##cursor_q_b", &cx, &cq, 1);
          ImPlot::Annotation(cx, ci, ImVec4(1, 1, 1, 1), ImVec2(10, -10), true, "%c I=%.4g", tag, ci);
          ImPlot::Annotation(cx, cq, ImVec4(1, 1, 1, 1), ImVec2(10, 10), true, "%c Q=%.4g", tag, cq);
        }
      });
}

} // namespace iqforge
