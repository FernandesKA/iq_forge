#pragma once

#include <imgui.h>
#include <implot.h>

namespace iqforge {

struct AxisZoomRequest {
  bool zoomInX = false;
  bool zoomOutX = false;
  bool zoomInY = false;
  bool zoomOutY = false;
};

struct AxisZoomState {
  bool valid = false;
  ImPlotRect limits{};
};

// Draws independent horizontal/vertical zoom buttons. Call before BeginPlot().
inline AxisZoomRequest drawAxisZoomButtons(bool enabled) {
  AxisZoomRequest req;
  ImGui::BeginDisabled(!enabled);
  req.zoomInX = ImGui::Button("H+");
  ImGui::SameLine();
  req.zoomOutX = ImGui::Button("H-");
  ImGui::SameLine();
  req.zoomInY = ImGui::Button("V+");
  ImGui::SameLine();
  req.zoomOutY = ImGui::Button("V-");
  ImGui::EndDisabled();
  return req;
}

// Applies a zoom request against the previous frame's captured limits.
// Call after SetupAxes() and before EndPlot().
inline void applyAxisZoom(const AxisZoomRequest& req, const AxisZoomState& state) {
  if (!state.valid) return;
  constexpr double kZoomInFactor = 0.8;
  constexpr double kZoomOutFactor = 1.25;

  auto zoomAxis = [](ImAxis axis, const ImPlotRange& range, double factor) {
    double center = (range.Min + range.Max) / 2.0;
    double halfSpan = (range.Max - range.Min) / 2.0 * factor;
    ImPlot::SetupAxisLimits(axis, center - halfSpan, center + halfSpan, ImPlotCond_Always);
  };

  if (req.zoomInX) zoomAxis(ImAxis_X1, state.limits.X, kZoomInFactor);
  if (req.zoomOutX) zoomAxis(ImAxis_X1, state.limits.X, kZoomOutFactor);
  if (req.zoomInY) zoomAxis(ImAxis_Y1, state.limits.Y, kZoomInFactor);
  if (req.zoomOutY) zoomAxis(ImAxis_Y1, state.limits.Y, kZoomOutFactor);
}

// Captures the current plot limits as the baseline for the next zoom step.
// Call right before EndPlot().
inline void captureAxisZoomState(AxisZoomState& state) {
  state.limits = ImPlot::GetPlotLimits();
  state.valid = true;
}

} // namespace iqforge
