#include "plot_waterfall.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

namespace iqforge {

void plotWaterfall(const char* plotId, const std::deque<std::vector<float>>& rows, AxisZoomState& zoom) {
  int numRows = static_cast<int>(rows.size());
  int cols = numRows > 0 ? static_cast<int>(rows.front().size()) : 0;

  if (numRows <= 0 || cols <= 0) {
    ImGui::TextDisabled("Waiting for data...");
    return;
  }

  static std::vector<float> flat;
  flat.resize(static_cast<size_t>(numRows) * cols);
  // Newest row on top: rows.back() is most recent.
  for (int r = 0; r < numRows; ++r) {
    const auto& src = rows[static_cast<size_t>(numRows - 1 - r)];
    std::copy(src.begin(), src.end(), flat.begin() + static_cast<long>(r) * cols);
  }

  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit");
  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 220), ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency bin", "History (newest at top)", ImPlotAxisFlags_NoTickLabels,
                       ImPlotAxisFlags_NoTickLabels);
    // The heatmap is drawn over the default [0,1]x[0,1] bounds (no custom
    // bounds_min/bounds_max passed below); stop panning/zooming past that.
    constrainAxisToData(ImAxis_X1, 0.0, 1.0, 0.0);
    constrainAxisToData(ImAxis_Y1, 0.0, 1.0, 0.0);
    applyAxisZoom(consumeWheelZoomRequest(zoom), zoom);
    ImPlot::PushColormap(ImPlotColormap_Viridis);
    ImPlot::PlotHeatmap(plotId, flat.data(), numRows, cols, -100.0, 0.0, nullptr);
    ImPlot::PopColormap();
    captureWheelZoomRequest(zoom);
    captureAxisZoomState(zoom);
    ImPlot::EndPlot();
  }
}

} // namespace iqforge
