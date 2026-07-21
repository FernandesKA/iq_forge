#include "plot_time_domain.h"

#include <imgui.h>
#include <implot.h>

namespace iqforge {

namespace {
void plotIQ(const char* plotId, const std::vector<Sample>& data) {
  if (ImPlot::BeginPlot(plotId, ImVec2(-1, 250))) {
    ImPlot::SetupAxes("Sample", "Amplitude");
    if (!data.empty()) {
      const float* base = reinterpret_cast<const float*>(data.data());
      int count = static_cast<int>(data.size());
      ImPlot::PlotLine("I", base, count, 1.0, 0.0, 0, 0, sizeof(Sample));
      ImPlot::PlotLine("Q", base + 1, count, 1.0, 0.0, 0, 0, sizeof(Sample));
    }
    ImPlot::EndPlot();
  }
}
} // namespace

void drawTimeDomainPanel(AppState& state) {
  ImGui::Begin("Time Domain");
  if (ImGui::BeginTabBar("TimeDomainTabs")) {
    if (ImGui::BeginTabItem("RX")) {
      plotIQ("##rx_time", state.rxTimeDomain);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("TX preview")) {
      plotIQ("##tx_time", state.txTimeDomain);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

} // namespace iqforge
