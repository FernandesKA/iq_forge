#include "plot_time_domain.h"

#include <imgui.h>
#include <implot.h>

#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
struct TimeDomainViewState {
  bool hadData = false;
  AxisZoomState zoom;
};

void plotIQ(const char* plotId, const std::vector<Sample>& data, TimeDomainViewState& view) {
  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  AxisZoomRequest zoomReq = drawAxisZoomButtons(view.zoom.valid && !data.empty());
  ImGui::SameLine();
  ImGui::TextDisabled("Wheel: zoom, drag: pan, double-click: fit, H/V: zoom one axis");

  if (data.empty()) view.hadData = false;
  if (!view.hadData && !data.empty()) fitRequested = true;
  view.hadData |= !data.empty();

  if (fitRequested && !data.empty()) ImPlot::SetNextAxesToFit();
  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 250))) {
    ImPlot::SetupAxes("Sample", "Amplitude");
    if (!fitRequested) applyAxisZoom(zoomReq, view.zoom);
    if (!data.empty()) {
      const float* base = reinterpret_cast<const float*>(data.data());
      int count = static_cast<int>(data.size());
      ImPlot::PlotLine("I", base, count, 1.0, 0.0, 0, 0, sizeof(Sample));
      ImPlot::PlotLine("Q", base + 1, count, 1.0, 0.0, 0, 0, sizeof(Sample));
    }
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
