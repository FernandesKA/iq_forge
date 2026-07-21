#include "plot_spectrum.h"

#include <imgui.h>
#include <implot.h>

namespace iqforge {

namespace {
void plotSpectrum(const char* plotId, const std::vector<float>& db, double sampleRateHz) {
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
  ImGui::Begin("Spectrum");
  if (ImGui::BeginTabBar("SpectrumTabs")) {
    if (ImGui::BeginTabItem("RX")) {
      plotSpectrum("##rx_spectrum", state.rxSpectrumDb, state.sampleRateHz);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TX preview")) {
      plotSpectrum("##tx_spectrum", state.txSpectrumDb, state.sampleRateHz);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

} // namespace iqforge
