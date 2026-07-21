#include "panel_rx.h"

#include <imgui.h>

namespace iqforge {

void drawRxPanel(AppState& state) {
  ImGui::Begin("RX");

  bool connected = state.deviceManager.isConnected();
  bool active = state.isRxActive();

  ImGui::BeginDisabled(!connected);
  if (!active) {
    if (ImGui::Button("Start RX")) {
      state.rxFft.resetAveraging();
      auto callback = [&state](const Sample* data, size_t count) {
        state.rxRing.pushLatest(SampleBuffer(data, data + count));
      };
      if (state.deviceManager.device()->startRx(callback, state.rxError)) {
        state.log("RX started");
      } else {
        state.log("RX start failed: " + state.rxError);
      }
    }
  } else {
    if (ImGui::Button("Stop RX")) {
      state.deviceManager.device()->stopRx();
      state.log("RX stopped");
    }
  }
  ImGui::EndDisabled();

  if (!state.rxError.empty()) {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", state.rxError.c_str());
  }

  ImGui::Separator();
  ImGui::Checkbox("Record to buffer", &state.rxRecording);
  ImGui::InputText("Path", state.rxRecordPathBuffer, sizeof(state.rxRecordPathBuffer));
  ImGui::Text("Buffered samples: %zu", state.rxRecordBuffer.size());

  ImGui::BeginDisabled(state.rxRecordBuffer.empty());
  if (ImGui::Button("Save & clear")) {
    try {
      saveIqFileCf32(state.rxRecordPathBuffer, state.rxRecordBuffer.data(), state.rxRecordBuffer.size());
      state.log(std::string("Saved recording to ") + state.rxRecordPathBuffer);
      state.rxRecordBuffer.clear();
    } catch (const std::exception& e) {
      state.log(std::string("Save failed: ") + e.what());
    }
  }
  ImGui::EndDisabled();

  ImGui::End();
}

} // namespace iqforge
