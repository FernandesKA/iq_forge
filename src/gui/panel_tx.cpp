#include "panel_tx.h"

#include <imgui.h>

#include <algorithm>
#include <memory>

namespace iqforge {

namespace {
const char* kWaveformNames[] = {"Tone", "Multi-tone", "Chirp / sweep", "Noise", "Ramp"};
}

void drawTxPanel(AppState& state) {
  ImGui::Begin("TX");

  bool connected = state.deviceManager.isConnected();
  bool active = state.isTxActive();

  ImGui::RadioButton("Signal generator", &state.txSourceMode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("IQ file", &state.txSourceMode, 1);

  ImGui::BeginDisabled(active);
  if (state.txSourceMode == 0) {
    int type = static_cast<int>(state.genConfig.type);
    if (ImGui::Combo("Waveform", &type, kWaveformNames, IM_ARRAYSIZE(kWaveformNames))) {
      state.genConfig.type = static_cast<WaveformType>(type);
    }
    ImGui::SliderFloat("Amplitude", &state.genConfig.amplitude, 0.0f, 1.0f);

    switch (state.genConfig.type) {
      case WaveformType::Tone:
        ImGui::InputDouble("Tone freq (Hz)", &state.genConfig.toneFreqHz, 0.0, 0.0, "%.0f");
        break;
      case WaveformType::MultiTone: {
        int n = static_cast<int>(state.genConfig.multiToneFreqsHz.size());
        if (ImGui::InputInt("Tone count", &n)) {
          n = std::clamp(n, 1, 8);
          state.genConfig.multiToneFreqsHz.resize(n, 100e3);
        }
        for (size_t i = 0; i < state.genConfig.multiToneFreqsHz.size(); ++i) {
          ImGui::PushID(static_cast<int>(i));
          ImGui::InputDouble("Freq (Hz)", &state.genConfig.multiToneFreqsHz[i], 0.0, 0.0, "%.0f");
          ImGui::PopID();
        }
        break;
      }
      case WaveformType::Chirp:
        ImGui::InputDouble("Start freq (Hz)", &state.genConfig.chirpStartFreqHz, 0.0, 0.0, "%.0f");
        ImGui::InputDouble("End freq (Hz)", &state.genConfig.chirpEndFreqHz, 0.0, 0.0, "%.0f");
        ImGui::InputDouble("Sweep duration (s)", &state.genConfig.chirpDurationSec, 0.0, 0.0, "%.6f");
        break;
      case WaveformType::Noise:
      case WaveformType::Ramp:
        break;
    }
  } else {
    ImGui::InputText("File path", state.filePathBuffer, sizeof(state.filePathBuffer));
    ImGui::Checkbox("Loop", &state.fileLoop);
    ImGui::TextDisabled("Formats: .cf32/.fc32 (float32 IQ), .ci16/.sc16 (int16 IQ), .wav (PCM16)");
  }
  ImGui::EndDisabled();

  ImGui::Separator();

  ImGui::BeginDisabled(!connected);
  if (!active) {
    if (ImGui::Button("Start TX")) {
      std::shared_ptr<ISampleSource> source;
      bool ok = true;
      if (state.txSourceMode == 0) {
        state.genConfig.sampleRateHz = state.sampleRateHz;
        state.generator->setConfig(state.genConfig);
        source = state.generator;
      } else {
        try {
          source = std::make_shared<IQFileSource>(IQFileSource::fromFile(state.filePathBuffer, state.fileLoop));
        } catch (const std::exception& e) {
          state.txError = e.what();
          state.log("TX file load failed: " + state.txError);
          ok = false;
        }
      }
      if (ok) {
        auto tee = std::make_shared<TeeSampleSource>(source, &state.txPreviewRing);
        if (state.deviceManager.device()->startTx(tee, state.txError)) {
          state.log("TX started");
        } else {
          state.log("TX start failed: " + state.txError);
        }
      }
    }
  } else {
    if (ImGui::Button("Stop TX")) {
      state.deviceManager.device()->stopTx();
      state.log("TX stopped");
    }
  }
  ImGui::EndDisabled();

  if (!state.txError.empty()) {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", state.txError.c_str());
  }

  ImGui::End();
}

} // namespace iqforge
