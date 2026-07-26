#include "plot_waterfall.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "plot_format.h"
#include "plot_spectrum.h"

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

struct WaterfallColormap {
  const char* name;
  ImPlotColormap map;
};
constexpr WaterfallColormap kWaterfallColormaps[] = {
    {"Viridis", ImPlotColormap_Viridis}, {"Plasma", ImPlotColormap_Plasma}, {"Hot", ImPlotColormap_Hot},
    {"Jet", ImPlotColormap_Jet},         {"Greys", ImPlotColormap_Greys},
};
constexpr int kNumWaterfallColormaps = static_cast<int>(sizeof(kWaterfallColormaps) / sizeof(kWaterfallColormaps[0]));

// Draws the color range/colormap/grid controls plus a compact readout of
// the most recent row, in a fixed-width column to the right of the plot --
// mirrors drawMeasurementsTable()'s layout next to the spectrum plot.
void drawWaterfallControls(WaterfallViewState& view, const std::vector<float>& latestRow, int numRows, int cols,
                            double sampleRateHz, double historySpanSec) {
  ImGui::Checkbox("Grid", &view.gridEnabled);

  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::BeginCombo("##colormap", kWaterfallColormaps[view.colormapIndex].name)) {
    for (int i = 0; i < kNumWaterfallColormaps; ++i) {
      bool isSel = (view.colormapIndex == i);
      if (ImGui::Selectable(kWaterfallColormaps[i].name, isSel)) view.colormapIndex = i;
      if (isSel) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::SetNextItemWidth(-1.0f);
  ImGui::DragFloat("##colormin", &view.colorMinDb, 1.0f, -160.0f, view.colorMaxDb - 1.0f, "Min %.0f dBFS");
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::DragFloat("##colormax", &view.colorMaxDb, 1.0f, view.colorMinDb + 1.0f, 20.0f, "Max %.0f dBFS");

  ImGui::Separator();

  if (!ImGui::BeginTable("##waterfall_measurements", 2, ImGuiTableFlags_SizingFixedFit)) return;
  auto row = [](const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value);
  };
  char buf[32];

  std::snprintf(buf, sizeof buf, "%d", cols);
  row("Bins", buf);
  std::snprintf(buf, sizeof buf, "%d", numRows);
  row("Rows", buf);
  row("History", formatSeconds(historySpanSec).c_str());

  if (!latestRow.empty()) {
    int n = static_cast<int>(latestRow.size());
    int peakBin = static_cast<int>(std::max_element(latestRow.begin(), latestRow.end()) - latestRow.begin());
    double peakFreqHz = -sampleRateHz / 2.0 + static_cast<double>(peakBin) * (sampleRateHz / n);
    std::snprintf(buf, sizeof buf, "%.1f dBFS", latestRow[static_cast<size_t>(peakBin)]);
    row("Last peak", buf);
    row("Last peak freq", formatHz(peakFreqHz).c_str());
  }

  ImGui::EndTable();
}
} // namespace

void plotWaterfall(const char* plotId, const std::deque<WaterfallRow>& rows, double sampleRateHz,
                    WaterfallViewState& view) {
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

  // Reserve the same right-hand width the spectrum plot's measurements
  // table takes up, so both plots' X axes span the same pixel range and a
  // signal centered in one is centered in the other. This has to be an
  // explicit outer child rather than passed straight to BeginPlot(): when
  // BeginPlot() itself is given a negative size, it wraps itself in a child
  // window sized by that value and then recomputes its frame size *again*
  // relative to that child's own (already-shrunk) available width, so a
  // large negative offset like this one gets subtracted twice. A -1 doesn't
  // suffer from this (see plot_spectrum.cpp's inner BeginPlot), but -238
  // visibly does.
  ImGui::BeginChild("##waterfall_plot", ImVec2(-kSpectrumMeasurementsWidth - 8.0f, 220), false,
                     ImGuiWindowFlags_NoScrollbar);
  ImPlotAxisFlags xFlags = ImPlotAxisFlags_NoTickLabels;
  ImPlotAxisFlags yFlags = ImPlotAxisFlags_None;
  if (view.gridEnabled) {
    xFlags |= ImPlotAxisFlags_Foreground;
    yFlags |= ImPlotAxisFlags_Foreground;
  } else {
    xFlags |= ImPlotAxisFlags_NoGridLines;
    yFlags |= ImPlotAxisFlags_NoGridLines;
  }
  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 220), ImPlotFlags_NoLegend)) {
    ImPlot::SetupAxes("Frequency bin", "Time", xFlags, yFlags);

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
    applyAxisZoom(consumeWheelZoomRequest(view.zoom), view.zoom);
    ImPlot::PushColormap(kWaterfallColormaps[view.colormapIndex].map);
    ImPlot::PlotHeatmap(plotId, flat.data(), numRows, cols, static_cast<double>(view.colorMinDb),
                         static_cast<double>(view.colorMaxDb), nullptr);
    ImPlot::PopColormap();
    captureWheelZoomRequest(view.zoom);
    captureAxisZoomState(view.zoom);
    ImPlot::EndPlot();
  }
  ImGui::EndChild();

  ImGui::SameLine();
  ImGui::BeginChild("##waterfall_measurements", ImVec2(kSpectrumMeasurementsWidth, 220), true);
  double historySpanSec = std::chrono::duration<double>(rows.back().timestamp - rows.front().timestamp).count();
  drawWaterfallControls(view, rows.back().db, numRows, cols, sampleRateHz, historySpanSec);
  ImGui::EndChild();
}

} // namespace iqforge
