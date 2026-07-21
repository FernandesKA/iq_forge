#include "freq_input.h"

#include <imgui.h>

namespace iqforge {

namespace {
constexpr double kMultipliers[4] = {1.0, 1e3, 1e6, 1e9};
constexpr const char* kNames[4] = {"Hz", "kHz", "MHz", "GHz"};
} // namespace

bool FrequencyInputHz(const char* label, double* hz, FreqUnit* unit) {
  bool changed = false;
  int unitIdx = static_cast<int>(*unit);

  ImGui::PushID(label);

  double display = *hz / kMultipliers[unitIdx];
  ImGui::SetNextItemWidth(140.0f);
  if (ImGui::InputDouble("##value", &display, 0.0, 0.0, "%.6g")) {
    *hz = display * kMultipliers[unitIdx];
    changed = true;
  }

  for (int i = 0; i < 4; ++i) {
    ImGui::SameLine(0.0f, 4.0f);
    bool selected = (unitIdx == i);
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::SmallButton(kNames[i])) {
      if (!selected) {
        // Treat the number already entered as a value in the newly selected
        // unit. For example, "2000" followed by kHz means 2,000,000 Hz.
        *hz = display * kMultipliers[i];
        *unit = static_cast<FreqUnit>(i);
        unitIdx = i;
        changed = true;
      }
    }
    if (selected) ImGui::PopStyleColor();
  }

  ImGui::SameLine();
  ImGui::TextUnformatted(label);

  ImGui::PopID();
  return changed;
}

} // namespace iqforge
