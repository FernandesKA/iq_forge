#pragma once

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "plot_zoom_controls.h"

namespace iqforge {

struct LineViewState {
  bool hadData = false;
  // Sentinel so the very first frame with data always counts as "changed"
  // below, triggering the initial fit.
  size_t lastCount = static_cast<size_t>(-1);
  AxisZoomState zoom;
};

// X-axis range shared by a family of line views (I/Q, phase, inst. freq)
// so that zooming/panning one keeps the others showing the same sample
// span. Bound to each plot via ImPlot::SetupAxisLinks(), which pulls this
// range in at the start of each plot's setup and pushes the plot's final
// (possibly drag-panned/zoomed) range back out at EndPlot -- so any zoom
// path (buttons, wheel, native drag-pan, native box-select) ends up synced
// here automatically without drawLineView needing to special-case each one.
// Defaults to a non-degenerate range so the first frame (before any data
// exists) doesn't hand ImPlot a zero-width axis.
struct SharedXAxisLink {
  double min = 0.0;
  double max = 1.0;
};

// A single sample index selected (Ctrl+click) on any plot in a family of
// line views, so the other views can mark the same x-position. `index` is
// in the shared "Sample" domain (0..count-1 of the I/Q/phase window); the
// instantaneous-frequency view has one fewer point and simply skips drawing
// the marker when index falls on that last, missing sample.
struct SampleCursorState {
  bool active = false;
  int index = 0;
};

namespace detail {

// Captures a Ctrl+click on the plot as the new shared sample selection.
// Must be called after the plot's axes are locked (e.g. after drawLines())
// so GetPlotMousePos() is valid.
inline void updateSampleCursor(SampleCursorState& cursor, size_t count) {
  if (count == 0 || !ImPlot::IsPlotHovered()) return;
  if (!ImGui::GetIO().KeyCtrl || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;
  ImPlotPoint mouse = ImPlot::GetPlotMousePos();
  long idx = std::lround(mouse.x);
  idx = std::max<long>(0, std::min<long>(idx, static_cast<long>(count) - 1));
  cursor.active = true;
  cursor.index = static_cast<int>(idx);
}

} // namespace detail

// Shared fit/zoom/pan-limit machinery for a "family of lines over a sample
// window" view -- I/Q, phase, and instantaneous frequency all take this
// shape. `count` is the number of x-positions (plotted at indices 0..count-1).
// Pass `resetView = true` the frame the underlying window changed
// discontinuously (e.g. trigger settings changed) so the plot re-fits
// instead of trying to smoothly zoom into what's now a different signal
// (this also drops any current sample selection, since it no longer
// corresponds to the same instant in the new window).
// `xLink` and `cursor` are shared across the whole family (same instances
// passed to every call) so zoom/pan and point-selection stay in sync
// between them. `computeYRange(lo, hi)` fills the data's Y span;
// `drawLines()` issues the actual ImPlot::PlotLine call(s) and, if it wants
// to mark the current `cursor` selection on its own series, may read it
// (it's updated by the time drawLines() runs). Call between
// BeginPlot()/EndPlot() is handled internally.
template <typename ComputeYRange, typename DrawLines>
void drawLineView(const char* plotId, const char* yLabel, size_t count, bool resetView, LineViewState& view,
                   SharedXAxisLink& xLink, SampleCursorState& cursor, ComputeYRange&& computeYRange,
                   DrawLines&& drawLines) {
  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  AxisZoomRequest zoomReq = drawAxisZoomButtons(view.zoom.valid && count > 0);
  mergeZoomRequest(zoomReq, consumeWheelZoomRequest(view.zoom));
  ImGui::SameLine();
  ImGui::TextDisabled(
      "Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit, H/V: zoom one axis, Ctrl+click: select "
      "sample");

  bool hasData = count > 0;
  if (!hasData) view.hadData = false;
  if ((!view.hadData && hasData) || resetView) fitRequested = true;
  // Keep re-fitting while `count` is still changing frame to frame -- e.g.
  // right after TX/RX starts, while the trigger's window (or the raw
  // rolling buffer, if the trigger is off) is still filling up from empty.
  // Fitting just once, on the very first frame any data exists, could lock
  // onto a tiny/incomplete initial batch (e.g. the first few samples of a
  // sine that hasn't swung negative yet) and never widen from there. Once
  // `count` stabilizes at its steady-state size, this stops firing and
  // manual zoom/pan takes over as usual.
  if (hasData && count != view.lastCount) fitRequested = true;
  view.lastCount = count;
  if (resetView) cursor.active = false;
  view.hadData |= hasData;

  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 220))) {
    ImPlot::SetupAxes("Sample", yLabel);
    ImPlot::SetupAxisLinks(ImAxis_X1, &xLink.min, &xLink.max);
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
    if (hasData) detail::updateSampleCursor(cursor, count);
    if (hasData && cursor.active && static_cast<size_t>(cursor.index) < count) {
      double cx = static_cast<double>(cursor.index);
      ImPlot::SetNextLineStyle(ImVec4(1.0f, 0.85f, 0.2f, 0.7f), 1.5f);
      ImPlot::PlotInfLines("##cursor_line", &cx, 1);
    }
    captureWheelZoomRequest(view.zoom);
    captureAxisZoomState(view.zoom);
    ImPlot::EndPlot();
  }
}

} // namespace iqforge
