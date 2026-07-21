#include "plot_spectrum.h"

#include <imgui.h>
#include <implot.h>

namespace iqforge {

namespace {
struct SpectrumViewState {
  bool hadData = false;
  double sampleRateHz = 0.0;
};

void plotSpectrum(const char* plotId, const std::vector<float>& db, double sampleRateHz,
                  SpectrumViewState& view) {
  bool fitRequested = ImGui::Button("Fit signal");
  ImGui::SameLine();
  ImGui::TextDisabled("Mouse wheel: zoom, drag: pan, double-click: fit");

  if (db.empty()) view.hadData = false;
  const bool scaleChanged = view.sampleRateHz != sampleRateHz;
  if ((!view.hadData && !db.empty()) || scaleChanged) fitRequested = true;
  view.hadData |= !db.empty();
  view.sampleRateHz = sampleRateHz;

  if (fitRequested && !db.empty()) ImPlot::SetNextAxesToFit();
  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 250))) {
    ImPlot::SetupAxes("Frequency (Hz, baseband)", "Power (dB)");
    if (!db.empty()) {
      int n = static_cast<int>(db.size());
      double xscale = sampleRateHz / n;
      double xstart = -sampleRateHz / 2.0;
      ImPlot::PlotLine("Spectrum", db.data(), n, xscale, xstart);
    }
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
