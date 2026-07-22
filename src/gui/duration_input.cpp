#include "duration_input.h"

#include <imgui.h>

#include "input_commit.h"

namespace iqforge {

namespace {
constexpr double kMultipliers[4] = {1e-9, 1e-6, 1e-3, 1.0};
constexpr const char* kNames[4] = {"ns", "\xc2\xb5s", "ms", "s"}; // µs in UTF-8
} // namespace

bool DurationInputSec(const char* label, double* seconds, TimeUnit* unit) {
  bool changed = false;
  int unitIdx = static_cast<int>(*unit);

  ImGui::PushID(label);

  double display = *seconds / kMultipliers[unitIdx];
  ImGui::SetNextItemWidth(140.0f);
  bool rawChanged = ImGui::InputDouble("##value", &display, 0.0, 0.0, "%.6g");
  if (rawChanged) *seconds = display * kMultipliers[unitIdx];
  // Only report a commit once editing settles (blur/Enter or idle timeout),
  // not on every keystroke.
  if (GateInputCommit(rawChanged)) changed = true;

  for (int i = 0; i < 4; ++i) {
    ImGui::SameLine(0.0f, 4.0f);
    bool selected = (unitIdx == i);
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    if (ImGui::SmallButton(kNames[i])) {
      if (!selected) {
        // Treat the number already entered as a value in the newly selected
        // unit. For example, "500" followed by ms means 0.5 s.
        *seconds = display * kMultipliers[i];
        *unit = static_cast<TimeUnit>(i);
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
