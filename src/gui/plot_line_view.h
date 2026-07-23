#pragma once

#include <imgui.h>
#include <implot.h>

#include "plot_zoom_controls.h"

namespace iqforge {

struct LineViewState {
  bool hadData = false;
  AxisZoomState zoom;
};

// Shared fit/zoom/pan-limit machinery for a "family of lines over a sample
// window" view -- I/Q, phase, and instantaneous frequency all take this
// shape. `count` is the number of x-positions (plotted at indices 0..count-1).
// Pass `resetView = true` the frame the underlying window changed
// discontinuously (e.g. trigger settings changed) so the plot re-fits
// instead of trying to smoothly zoom into what's now a different signal.
// `computeYRange(lo, hi)` fills the data's Y span; `drawLines()` issues the
// actual ImPlot::PlotLine call(s). Call between BeginPlot()/EndPlot() is
// handled internally.
template <typename ComputeYRange, typename DrawLines>
void drawLineView(const char* plotId, const char* yLabel, size_t count, bool resetView, LineViewState& view,
                   ComputeYRange&& computeYRange, DrawLines&& drawLines) {
  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  AxisZoomRequest zoomReq = drawAxisZoomButtons(view.zoom.valid && count > 0);
  mergeZoomRequest(zoomReq, consumeWheelZoomRequest(view.zoom));
  ImGui::SameLine();
  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit, H/V: zoom one axis");

  bool hasData = count > 0;
  if (!hasData) view.hadData = false;
  if ((!view.hadData && hasData) || resetView) fitRequested = true;
  view.hadData |= hasData;

  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 220))) {
    ImPlot::SetupAxes("Sample", yLabel);
    if (hasData) {
      double lo = 0.0, hi = 0.0;
      computeYRange(lo, hi);
      constrainAxisToData(ImAxis_X1, 0.0, static_cast<double>(count - 1), 0.05);
      constrainAxisToData(ImAxis_Y1, lo, hi, 0.1);
      if (fitRequested) {
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(count - 1), ImPlotCond_Always);
        fitAxisWithMargin(ImAxis_Y1, lo, hi, 0.1);
      }
    }
    if (!fitRequested) applyAxisZoom(zoomReq, view.zoom);
    if (hasData) drawLines();
    captureWheelZoomRequest(view.zoom);
    captureAxisZoomState(view.zoom);
    ImPlot::EndPlot();
  }
}

} // namespace iqforge
