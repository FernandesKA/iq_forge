#include "plot_time_domain.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

#include "plot_trigger.h"
#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
struct TimeDomainViewState {
  bool hadData = false;
  AxisZoomState zoom;
  TriggerState trigger;
};

void plotIQ(const char* plotId, const std::vector<Sample>& data, TimeDomainViewState& view) {
  bool resetFromTrigger = drawTriggerControls(view.trigger);

  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  AxisZoomRequest zoomReq = drawAxisZoomButtons(view.zoom.valid && !data.empty());
  mergeZoomRequest(zoomReq, consumeWheelZoomRequest(view.zoom));
  ImGui::SameLine();
  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit, H/V: zoom one axis");

  auto [plotData, plotCount] = applyTrigger(data, view.trigger);
  bool hasData = plotCount > 0;

  if (!hasData) view.hadData = false;
  if ((!view.hadData && hasData) || resetFromTrigger) fitRequested = true;
  view.hadData |= hasData;

  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 250))) {
    ImPlot::SetupAxes("Sample", "Amplitude");
    if (hasData) {
      float lo = plotData[0].real(), hi = plotData[0].real();
      for (size_t i = 0; i < plotCount; ++i) {
        lo = std::min({lo, plotData[i].real(), plotData[i].imag()});
        hi = std::max({hi, plotData[i].real(), plotData[i].imag()});
      }
      constrainAxisToData(ImAxis_X1, 0.0, static_cast<double>(plotCount - 1), 0.05);
      constrainAxisToData(ImAxis_Y1, lo, hi, 0.1);
      if (fitRequested) {
        // X (sample index) fits flush; Y gets the same margin as the pan
        // limit above so the waveform isn't drawn right up to the border.
        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0, static_cast<double>(plotCount - 1), ImPlotCond_Always);
        fitAxisWithMargin(ImAxis_Y1, lo, hi, 0.1);
      }
    }
    if (!fitRequested) applyAxisZoom(zoomReq, view.zoom);
    if (hasData) {
      const float* base = reinterpret_cast<const float*>(plotData);
      int count = static_cast<int>(plotCount);
      ImPlot::PlotLine("I", base, count, 1.0, 0.0, 0, 0, sizeof(Sample));
      ImPlot::PlotLine("Q", base + 1, count, 1.0, 0.0, 0, 0, sizeof(Sample));
    }
    captureWheelZoomRequest(view.zoom);
    captureAxisZoomState(view.zoom);
    ImPlot::EndPlot();
  }
}
} // namespace

void drawTimeDomainPanel(AppState& state) {
  static TimeDomainViewState rxView;
  static TimeDomainViewState txView;

  ImGui::Begin("Time Domain");
  if (ImGui::BeginTabBar("TimeDomainTabs")) {
    if (ImGui::BeginTabItem("RX")) {
      plotIQ("##rx_time", state.rxTimeDomain, rxView);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TX preview")) {
      plotIQ("##tx_time", state.txTimeDomain, txView);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

} // namespace iqforge
