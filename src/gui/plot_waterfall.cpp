#include "plot_waterfall.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

#include "plot_zoom_controls.h"

namespace iqforge {

void drawWaterfallPanel(AppState& state) {
  static AxisZoomState zoom;

  ImGui::Begin("RX Waterfall");
  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit");

  int rows = static_cast<int>(state.waterfallRows.size());
  int cols = rows > 0 ? static_cast<int>(state.waterfallRows.front().size()) : 0;

  if (rows > 0 && cols > 0) {
    static std::vector<float> flat;
    flat.resize(static_cast<size_t>(rows) * cols);
    // Newest row on top: waterfallRows.back() is most recent.
    for (int r = 0; r < rows; ++r) {
      const auto& src = state.waterfallRows[static_cast<size_t>(rows - 1 - r)];
      std::copy(src.begin(), src.end(), flat.begin() + static_cast<long>(r) * cols);
    }

    if (ImPlot::BeginPlot("##waterfall", ImVec2(-1, 300), ImPlotFlags_NoLegend)) {
      ImPlot::SetupAxes("Frequency bin", "History (newest at top)", ImPlotAxisFlags_NoTickLabels,
                         ImPlotAxisFlags_NoTickLabels);
      // The heatmap is drawn over the default [0,1]x[0,1] bounds (no custom
      // bounds_min/bounds_max passed below); stop panning/zooming past that.
      constrainAxisToData(ImAxis_X1, 0.0, 1.0, 0.0);
      constrainAxisToData(ImAxis_Y1, 0.0, 1.0, 0.0);
      applyAxisZoom(consumeWheelZoomRequest(zoom), zoom);
      ImPlot::PushColormap(ImPlotColormap_Viridis);
      ImPlot::PlotHeatmap("waterfall", flat.data(), rows, cols, -100.0, 0.0, nullptr);
      ImPlot::PopColormap();
      captureWheelZoomRequest(zoom);
      captureAxisZoomState(zoom);
      ImPlot::EndPlot();
    }
  } else {
    ImGui::TextDisabled("Waiting for RX data...");
  }

  ImGui::End();
}

} // namespace iqforge
