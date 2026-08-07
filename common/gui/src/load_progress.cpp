#include "load_progress.h"

#include <imgui.h>

#include <cstdio>

namespace iqforge {

void drawLoadProgressBar(const AsyncIqLoadJob& job) {
  float frac = job.progress();
  char label[64];
  std::snprintf(label, sizeof label, "%s %.0f%%", job.stage(), frac * 100.0f);

  // Deliberately not using ProgressBar()'s own overlay-text parameter: Dear
  // ImGui positions that text right at the fill edge (ImLerp(bb.Min, bb.Max,
  // fraction) in ProgressBar()'s implementation), so it visibly rides along
  // as the fraction grows instead of staying put. Drawing the label
  // ourselves, fixed at the bar's center regardless of fill, is what
  // actually reads as a stable percentage readout rather than something
  // sliding across the screen.
  ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), "");
  ImVec2 barMin = ImGui::GetItemRectMin();
  ImVec2 barMax = ImGui::GetItemRectMax();
  ImVec2 labelSize = ImGui::CalcTextSize(label);
  ImVec2 labelPos((barMin.x + barMax.x - labelSize.x) * 0.5f, (barMin.y + barMax.y - labelSize.y) * 0.5f);
  ImGui::GetWindowDrawList()->AddText(labelPos, ImGui::GetColorU32(ImGuiCol_Text), label);
}

} // namespace iqforge
