#include "plot_spectrum.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>

#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
struct SpectrumViewState {
  bool hadData = false;
  double sampleRateHz = 0.0;
  AxisZoomState zoom;
};

void plotSpectrum(const char* plotId, const std::vector<float>& db, double sampleRateHz,
                  SpectrumViewState& view) {
  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  AxisZoomRequest zoomReq = drawAxisZoomButtons(view.zoom.valid && !db.empty());
  mergeZoomRequest(zoomReq, consumeWheelZoomRequest(view.zoom));
  ImGui::SameLine();
  ImGui::TextDisabled("Wheel: zoom X, Shift+wheel: zoom Y, drag: pan, double-click: fit, H/V: zoom one axis");

  if (db.empty()) view.hadData = false;
  const bool scaleChanged = view.sampleRateHz != sampleRateHz;
  if ((!view.hadData && !db.empty()) || scaleChanged) fitRequested = true;
  view.hadData |= !db.empty();
  view.sampleRateHz = sampleRateHz;

  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 250))) {
    ImPlot::SetupAxes("Frequency (Hz, baseband)", "Power (dB)");
    if (!db.empty()) {
      auto [minIt, maxIt] = std::minmax_element(db.begin(), db.end());
      constrainAxisToData(ImAxis_X1, -sampleRateHz / 2.0, sampleRateHz / 2.0, 0.02);
      constrainAxisToData(ImAxis_Y1, *minIt, *maxIt, 0.1);
      if (fitRequested) {
        // X (frequency) fits flush; Y gets the same margin as the pan
        // limit above so the curve isn't drawn right up to the border.
        ImPlot::SetupAxisLimits(ImAxis_X1, -sampleRateHz / 2.0, sampleRateHz / 2.0, ImPlotCond_Always);
        fitAxisWithMargin(ImAxis_Y1, *minIt, *maxIt, 0.1);
      }
    }
    if (!fitRequested) applyAxisZoom(zoomReq, view.zoom);
    if (!db.empty()) {
      int n = static_cast<int>(db.size());
      double xscale = sampleRateHz / n;
      double xstart = -sampleRateHz / 2.0;
      ImPlot::PlotLine("Spectrum", db.data(), n, xscale, xstart);
    }
    captureWheelZoomRequest(view.zoom);
    captureAxisZoomState(view.zoom);
    ImPlot::EndPlot();
  }
}
} // namespace

void drawSpectrumPanel(AppState& state) {
  static SpectrumViewState rxView;
  static SpectrumViewState txView;

  ImGui::Begin("Spectrum");
  if (ImGui::BeginTabBar("SpectrumTabs")) {
    if (ImGui::BeginTabItem("RX")) {
      plotSpectrum("##rx_spectrum", state.rxSpectrumDb, state.sampleRateHz, rxView);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TX preview")) {
      plotSpectrum("##tx_spectrum", state.txSpectrumDb, state.sampleRateHz, txView);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

} // namespace iqforge
