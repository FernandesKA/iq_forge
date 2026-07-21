#include "plot_waterfall.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

namespace iqforge {

void drawWaterfallPanel(AppState& state) {
  ImGui::Begin("RX Waterfall");

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
      ImPlot::PushColormap(ImPlotColormap_Viridis);
      ImPlot::PlotHeatmap("waterfall", flat.data(), rows, cols, -100.0, 0.0, nullptr);
      ImPlot::PopColormap();
      ImPlot::EndPlot();
    }
  } else {
    ImGui::TextDisabled("Waiting for RX data...");
  }

  ImGui::End();
}

} // namespace iqforge
