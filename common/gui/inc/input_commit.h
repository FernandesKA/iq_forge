#pragma once

#include <imgui.h>

namespace iqforge {

// How long an input field may sit idle mid-edit (no keystrokes, but still
// focused) before its value is treated as committed.
constexpr float kInputCommitTimeoutSec = 0.7f;

// Gates a live-typed InputXxx widget so callers only see a "commit" once
// editing settles, instead of on every keystroke. Without this, typing
// "712" into a field bound directly to hardware (e.g. center frequency)
// retunes the device once per character: 700 -> 70 -> 7 -> 71 -> 712.
//
// Call immediately after the InputXxx widget so IsItemDeactivatedAfterEdit()
// still refers to it. `rawChanged` is that widget's own per-keystroke return
// value. Returns true when the field should be committed: on
// Enter/Tab/click-away, or after kInputCommitTimeoutSec of inactivity since
// the last keystroke.
inline bool GateInputCommit(bool rawChanged) {
  ImGuiStorage* storage = ImGui::GetStateStorage();
  const ImGuiID dirtySinceId = ImGui::GetID("##dirtySince");
  float dirtySince = storage->GetFloat(dirtySinceId, -1.0f);

  if (rawChanged) {
    dirtySince = static_cast<float>(ImGui::GetTime());
    storage->SetFloat(dirtySinceId, dirtySince);
  }

  const bool deactivated = ImGui::IsItemDeactivatedAfterEdit();
  const bool timedOut =
      dirtySince >= 0.0f && (ImGui::GetTime() - dirtySince) >= kInputCommitTimeoutSec;

  if (deactivated || timedOut) {
    storage->SetFloat(dirtySinceId, -1.0f);
    return true;
  }
  return false;
}

} // namespace iqforge
