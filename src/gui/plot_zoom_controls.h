#pragma once

#include <imgui.h>
#include <implot.h>

#include <cmath>
#include <limits>

namespace iqforge {

struct AxisZoomRequest {
  bool zoomInX = false;
  bool zoomOutX = false;
  bool zoomInY = false;
  bool zoomOutY = false;
  // Plot-space point to keep fixed while zooming (e.g. the mouse cursor
  // during a wheel-zoom), so that spot stays under the cursor instead of
  // the view just growing/shrinking around its center. NaN ("not set",
  // the default -- what the H/V buttons leave it as) falls back to
  // centering on the current view, same as before.
  double anchorX = std::numeric_limits<double>::quiet_NaN();
  double anchorY = std::numeric_limits<double>::quiet_NaN();
};

struct AxisZoomState {
  bool valid = false;
  ImPlotRect limits{};
  // Wheel input is only known to be over *this* plot once ImPlot's setup
  // phase is locked (IsPlotHovered() forces that), which is too late in the
  // frame to still call SetupAxisLimits(). So it's captured post-lock (see
  // captureWheelZoomRequest(), called near EndPlot()) and applied at the
  // top of the *next* frame instead, the same one-frame-lag the H/V buttons
  // already rely on via `limits` above.
  AxisZoomRequest pendingWheel;
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

// Pulls in the wheel-driven request captured last frame (see
// captureWheelZoomRequest below) and clears it. Call before BeginPlot(),
// alongside drawAxisZoomButtons(), and OR the result into that button
// request before applying either.
inline AxisZoomRequest consumeWheelZoomRequest(AxisZoomState& state) {
  AxisZoomRequest req = state.pendingWheel;
  state.pendingWheel = AxisZoomRequest{};
  return req;
}

inline void mergeZoomRequest(AxisZoomRequest& dst, const AxisZoomRequest& src) {
  dst.zoomInX |= src.zoomInX;
  dst.zoomOutX |= src.zoomOutX;
  dst.zoomInY |= src.zoomInY;
  dst.zoomOutY |= src.zoomOutY;
  if (src.zoomInX || src.zoomOutX) dst.anchorX = src.anchorX;
  if (src.zoomInY || src.zoomOutY) dst.anchorY = src.anchorY;
}

// Applies a zoom request against the previous frame's captured limits.
// Call after SetupAxes() and before EndPlot().
inline void applyAxisZoom(const AxisZoomRequest& req, const AxisZoomState& state) {
  if (!state.valid) return;
  constexpr double kZoomInFactor = 0.8;
  constexpr double kZoomOutFactor = 1.25;

  // Keeps `anchor` fixed in plot-space while the range scales by `factor`
  // (e.g. the point under the cursor stays under the cursor). Falls back
  // to the range's own center when no anchor was captured (button clicks).
  auto zoomAxis = [](ImAxis axis, const ImPlotRange& range, double factor, double anchor) {
    double pivot = std::isnan(anchor) ? (range.Min + range.Max) / 2.0 : anchor;
    double newMin = pivot - (pivot - range.Min) * factor;
    double newMax = pivot + (range.Max - pivot) * factor;
    ImPlot::SetupAxisLimits(axis, newMin, newMax, ImPlotCond_Always);
  };

  if (req.zoomInX) zoomAxis(ImAxis_X1, state.limits.X, kZoomInFactor, req.anchorX);
  if (req.zoomOutX) zoomAxis(ImAxis_X1, state.limits.X, kZoomOutFactor, req.anchorX);
  if (req.zoomInY) zoomAxis(ImAxis_Y1, state.limits.Y, kZoomInFactor, req.anchorY);
  if (req.zoomOutY) zoomAxis(ImAxis_Y1, state.limits.Y, kZoomOutFactor, req.anchorY);
}

// Captures the current plot limits as the baseline for the next zoom step.
// Call right before EndPlot().
inline void captureAxisZoomState(AxisZoomState& state) {
  state.limits = ImPlot::GetPlotLimits();
  state.valid = true;
}

// Captures mouse-wheel scrolling over the plot for next frame's zoom: no
// modifier zooms the X axis, Shift zooms the Y axis -- matching ImPlot's
// own convention where Shift already means "vertical" for box-select.
// ImPlot's built-in combined wheel-zoom must be disabled globally (see
// configurePlotWheelZoom()) or it fires at the same time and fights this.
// Call between BeginPlot() and EndPlot(), anywhere after the plot's own
// Setup*() calls (this forces the setup phase to lock, like PlotLine does).
inline void captureWheelZoomRequest(AxisZoomState& state) {
  state.pendingWheel = AxisZoomRequest{};
  if (!ImPlot::IsPlotHovered()) return;
  float wheel = ImGui::GetIO().MouseWheel;
  if (wheel == 0.0f) return;

  // Valid here because IsPlotHovered() above already forced the setup
  // phase to lock, so this frame's axis scale is established.
  ImPlotPoint mouse = ImPlot::GetPlotMousePos();

  bool vertical = ImGui::GetIO().KeyShift;
  bool zoomIn = wheel > 0.0f;
  if (vertical) {
    if (zoomIn) state.pendingWheel.zoomInY = true; else state.pendingWheel.zoomOutY = true;
    state.pendingWheel.anchorY = mouse.y;
  } else {
    if (zoomIn) state.pendingWheel.zoomInX = true; else state.pendingWheel.zoomOutX = true;
    state.pendingWheel.anchorX = mouse.x;
  }
}

// Call once at startup (after ImPlot::CreateContext()). Requires all four
// modifiers together for ImPlot's own built-in wheel-zoom (which always
// zooms both axes at once) so it never fires in practice, leaving plain
// wheel / Shift+wheel entirely to captureWheelZoomRequest() above.
inline void configurePlotWheelZoom() {
  ImPlot::GetInputMap().ZoomMod = ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiMod_Alt | ImGuiMod_Super;
}

// Stops panning/zooming (wheel, drag, or the H/V buttons above) from going
// past [lo, hi] -- the actual range spanned by what's drawn -- plus a small
// margin so the curve isn't flush against the plot border. Without this,
// zooming/panning out keeps going into blank space beyond the data. Call
// after SetupAxes(), before EndPlot(). No-op if the range is degenerate
// (e.g. a single-sample buffer).
inline void constrainAxisToData(ImAxis axis, double lo, double hi, double marginFrac = 0.05) {
  if (!(hi > lo)) return;
  double margin = (hi - lo) * marginFrac;
  ImPlot::SetupAxisLimitsConstraints(axis, lo - margin, hi + margin);
}

// Fits an axis to exactly [lo, hi] plus a margin, for use when "fitting" a
// plot to its data. Unlike ImPlot's own SetNextAxesToFit(), which fits
// flush with the curve touching the plot border, this leaves breathing
// room -- the same margin constrainAxisToData() above uses for the pan/zoom
// limit, so a fully-fit view sits exactly at that limit. Call after
// SetupAxes(), before EndPlot(). Falls back to a small fixed margin around
// a degenerate (single-value) range so the view isn't zero-height.
inline void fitAxisWithMargin(ImAxis axis, double lo, double hi, double marginFrac = 0.1) {
  double margin = (hi > lo) ? (hi - lo) * marginFrac : 1.0;
  ImPlot::SetupAxisLimits(axis, lo - margin, hi + margin, ImPlotCond_Always);
}

} // namespace iqforge
