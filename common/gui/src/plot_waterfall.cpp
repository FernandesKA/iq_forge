#include "plot_waterfall.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace iqforge {

namespace {
void formatAgo(std::chrono::steady_clock::time_point rowTime, std::chrono::steady_clock::time_point now, char* buf,
               size_t bufSize) {
  double secondsAgo = std::chrono::duration<double>(now - rowTime).count();
  if (secondsAgo < 0.05) {
    std::snprintf(buf, bufSize, "now");
  } else {
    std::snprintf(buf, bufSize, "-%.1fs", secondsAgo);
  }
}
} // namespace

void plotWaterfall(const char* plotId, const std::deque<WaterfallRow>& rows, AxisZoomState& zoom) {
  int numRows = static_cast<int>(rows.size());
  int cols = numRows > 0 ? static_cast<int>(rows.front().db.size()) : 0;

  if (numRows <= 0 || cols <= 0) {
    ImGui::TextDisabled("Waiting for data...");
    return;
  }

  static std::vector<float> flat;
  flat.resize(static_cast<size_t>(numRows) * cols);
  // Newest row on top: rows.back() is most recent.
  for (int r = 0; r < numRows; ++r) {
    const auto& src = rows[static_cast<size_t>(numRows - 1 - r)].db;
    std::copy(src.begin(), src.end(), flat.begin() + static_cast<long>(r) * cols);
  }

  auto now = std::chrono::steady_clock::now();
  double newestAgoSec = std::chrono::duration<double>(now - rows.back().timestamp).count();
  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit");
  ImGui::SameLine();
  ImGui::TextDisabled("| last row: %.1fs ago", newestAgoSec);

  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 220), ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency bin", "Time", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_None);

    // Label the Y axis with how long ago each row was captured (row 0 is
    // newest/top, see the flat-array fill above) instead of just "newest at
    // top" -- otherwise there's no way to tell how deep the history goes or
    // whether it's actually advancing.
    constexpr int kNumTicks = 5;
    double tickValues[kNumTicks];
    char tickLabels[kNumTicks][16];
    const char* tickLabelPtrs[kNumTicks];
    for (int i = 0; i < kNumTicks; ++i) {
      double frac = static_cast<double>(i) / (kNumTicks - 1); // 0 = bottom (oldest), 1 = top (newest)
      tickValues[i] = frac;
      int flatRow = static_cast<int>(std::lround((1.0 - frac) * (numRows - 1))); // 0 = newest
      size_t origIdx = static_cast<size_t>(numRows - 1 - flatRow);
      formatAgo(rows[origIdx].timestamp, now, tickLabels[i], sizeof(tickLabels[i]));
      tickLabelPtrs[i] = tickLabels[i];
    }
    ImPlot::SetupAxisTicks(ImAxis_Y1, tickValues, kNumTicks, tickLabelPtrs);

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
