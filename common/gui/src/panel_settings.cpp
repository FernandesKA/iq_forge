#include "panel_settings.h"

#include <imgui.h>

#include <string>
#include <vector>

#include "app_settings.h"
#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
char presetNameBuffer[128] = "";
} // namespace

void drawSettingsPanel(AppState& state) {
  ImGui::Begin("Settings");

  static bool autoSaveEnabled = loadAutoSaveEnabled();
  if (ImGui::Checkbox("Save settings on exit / restore on startup", &autoSaveEnabled)) {
    saveAutoSaveEnabled(autoSaveEnabled);
  }
  ImGui::SameLine();
  HelpMarker(
      "When enabled, the current device/TX/RX configuration is saved when "
      "the app closes and restored the next time it starts.\n"
      "Disabling this does not touch named presets below -- those can "
      "always be saved and loaded regardless of this setting.");

  ImGui::Separator();
  ImGui::TextUnformatted("Main area tabs");
  ImGui::SameLine();
  HelpMarker("Which plot/preview windows show up in the main area's tab bar. Each *Control panel on the left stays available regardless.");
  ImGui::Checkbox("TX##showtab", &state.showTxTab);
  sameLineOrWrap(wrapButtonWidth("RX##showtab"));
  ImGui::Checkbox("RX##showtab", &state.showRxTab);
  sameLineOrWrap(wrapButtonWidth("Signal Viewer##showtab"));
  ImGui::Checkbox("Signal Viewer##showtab", &state.showSignalViewerTab);
  sameLineOrWrap(wrapButtonWidth("SpectrumViewer##showtab"));
  ImGui::Checkbox("SpectrumViewer##showtab", &state.showSpectrumViewerTab);

  ImGui::Separator();
  ImGui::TextUnformatted("Presets");

  // Cached rather than re-read from disk every frame; refreshed explicitly
  // right after any action below that could change it.
  static std::vector<std::string> presets = listPresets();

  ImGui::SetNextItemWidth(200.0f);
  ImGui::InputTextWithHint("##presetName", "Preset name", presetNameBuffer, sizeof(presetNameBuffer));
  sameLineOrWrap(wrapButtonWidth("Save preset"));
  ImGui::BeginDisabled(presetNameBuffer[0] == '\0');
  if (ImGui::Button("Save preset")) {
    std::string name = presetNameBuffer;
    savePreset(state, name);
    state.log("Saved preset \"" + name + "\"");
    presets = listPresets();
  }
  ImGui::EndDisabled();

  if (presets.empty()) {
    ImGui::TextDisabled("No presets saved yet");
  }

  // Deletion is deferred until after the loop below: erasing from `presets`
  // (via a re-fetch) while a range-based for is iterating over it would
  // invalidate the loop's iterators mid-iteration.
  std::string pendingDelete;
  for (const auto& name : presets) {
    ImGui::PushID(name.c_str());
    ImGui::TextUnformatted(name.c_str());
    sameLineOrWrap(wrapButtonWidth("Load") + wrapButtonWidth("Delete") + 24.0f);
    if (ImGui::Button("Load")) {
      if (loadPreset(state, name)) {
        state.log("Loaded preset \"" + name + "\"");
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) pendingDelete = name;
    ImGui::PopID();
  }
  if (!pendingDelete.empty()) {
    deletePreset(pendingDelete);
    state.log("Deleted preset \"" + pendingDelete + "\"");
    presets = listPresets();
  }

  ImGui::End();
}

} // namespace iqforge
