#include "log_panel.h"

#include <imgui.h>

namespace iqforge {

void drawLogPanel(AppState& state) {
  ImGui::Begin("Log");
  {
    std::lock_guard<std::mutex> lock(state.logMutex);
    for (const auto& line : state.logMessages) {
      ImGui::TextUnformatted(line.c_str());
    }
  }
  if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }
  ImGui::End();
}

} // namespace iqforge
