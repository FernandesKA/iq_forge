#include "panel_tx.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <memory>

#include "freq_input.h"

namespace iqforge {

namespace {
const char* kWaveformNames[] = {"Tone", "Multi-tone", "Chirp / sweep", "Barker", "Noise", "Ramp"};
const char* kBarkerCodeNames[] = {
    "B2  [+ -]",       "B2  [+ +]",       "B3  [+ + -]",
    "B4  [+ + - +]",   "B4  [+ + + -]",   "B5  [+ + + - +]",
    "B7  [+ + + - - + -]", "B11 [+ + + - - - + - - + -]",
    "B13 [+ + + + + - - + + - + - +]"};

bool clampGeneratorFrequencies(GeneratorConfig& cfg, double sampleRateHz) {
  const double nyquistHz = std::abs(sampleRateHz) * 0.5;
  const auto clampFrequency = [nyquistHz](double& hz) {
    const double clamped = std::clamp(hz, -nyquistHz, nyquistHz);
    if (clamped == hz) return false;
    hz = clamped;
    return true;
  };

  bool changed = clampFrequency(cfg.toneFreqHz);
  for (double& hz : cfg.multiToneFreqsHz) changed |= clampFrequency(hz);
  changed |= clampFrequency(cfg.chirpStartFreqHz);
  changed |= clampFrequency(cfg.chirpEndFreqHz);
  const double clampedChipRate = std::clamp(cfg.barkerChipRateHz, 0.0, std::abs(sampleRateHz));
  if (clampedChipRate != cfg.barkerChipRateHz) {
    cfg.barkerChipRateHz = clampedChipRate;
    changed = true;
  }
  return changed;
}
}

void drawTxPanel(AppState& state) {
  ImGui::Begin("TX");

  bool connected = state.deviceManager.isConnected();
  bool active = state.isTxActive();

  ImGui::BeginDisabled(active);
  ImGui::RadioButton("Signal generator", &state.txSourceMode, 0);
  ImGui::SameLine();
  ImGui::RadioButton("IQ file", &state.txSourceMode, 1);
  ImGui::EndDisabled();

  if (state.txSourceMode == 0) {
    bool generatorChanged = clampGeneratorFrequencies(state.genConfig, state.sampleRateHz);
    if (state.genConfig.sampleRateHz != state.sampleRateHz) {
      state.genConfig.sampleRateHz = state.sampleRateHz;
      generatorChanged = true;
    }
    const double nyquistHz = std::abs(state.sampleRateHz) * 0.5;
    int type = static_cast<int>(state.genConfig.type);
    if (ImGui::Combo("Waveform", &type, kWaveformNames, IM_ARRAYSIZE(kWaveformNames))) {
      state.genConfig.type = static_cast<WaveformType>(type);
      generatorChanged = true;
    }
    generatorChanged |= ImGui::SliderFloat("Amplitude", &state.genConfig.amplitude, 0.0f, 1.0f);

    switch (state.genConfig.type) {
      case WaveformType::Tone:
        generatorChanged |= FrequencyInputHz("Tone freq", &state.genConfig.toneFreqHz, &state.toneFreqUnit);
        break;
      case WaveformType::MultiTone: {
        int n = static_cast<int>(state.genConfig.multiToneFreqsHz.size());
        if (ImGui::InputInt("Tone count", &n)) {
          n = std::clamp(n, 1, 8);
          state.genConfig.multiToneFreqsHz.resize(n, 100e3);
          generatorChanged = true;
        }
        for (size_t i = 0; i < state.genConfig.multiToneFreqsHz.size(); ++i) {
          ImGui::PushID(static_cast<int>(i));
          generatorChanged |=
              FrequencyInputHz("Freq", &state.genConfig.multiToneFreqsHz[i], &state.multiToneUnit);
          ImGui::PopID();
        }
        break;
      }
      case WaveformType::Chirp:
        generatorChanged |=
            FrequencyInputHz("Start freq", &state.genConfig.chirpStartFreqHz, &state.chirpStartUnit);
        generatorChanged |=
            FrequencyInputHz("End freq", &state.genConfig.chirpEndFreqHz, &state.chirpEndUnit);
        generatorChanged |= ImGui::InputDouble(
            "Sweep duration (s)", &state.genConfig.chirpDurationSec, 0.0, 0.0, "%.6f");
        break;
      case WaveformType::Barker: {
        int code = static_cast<int>(state.genConfig.barkerCode);
        if (ImGui::Combo("Barker code", &code, kBarkerCodeNames, IM_ARRAYSIZE(kBarkerCodeNames))) {
          state.genConfig.barkerCode = static_cast<BarkerCode>(code);
          generatorChanged = true;
        }
        generatorChanged |= FrequencyInputHz(
            "Chip rate", &state.genConfig.barkerChipRateHz, &state.barkerChipRateUnit);
        break;
      }
      case WaveformType::Noise:
      case WaveformType::Ramp:
        break;
    }

    // Complex IQ can represent baseband offsets only inside the Nyquist
    // interval. Values outside it would wrap to an unexpected RF frequency.
    generatorChanged |= clampGeneratorFrequencies(state.genConfig, state.sampleRateHz);
    if (state.genConfig.type == WaveformType::Barker) {
      ImGui::TextDisabled("Allowed chip rate: 0 .. %.6g MHz", std::abs(state.sampleRateHz) / 1e6);
    } else {
      ImGui::TextDisabled("Allowed frequency offset: %.6g .. +%.6g MHz",
                          -nyquistHz / 1e6, nyquistHz / 1e6);
    }

    if (active && generatorChanged) {
      state.genConfig.sampleRateHz = state.sampleRateHz;
      state.generator->setConfig(state.genConfig);
    }
  } else {
    ImGui::BeginDisabled(active);
    ImGui::InputText("File path", state.filePathBuffer, sizeof(state.filePathBuffer));
    ImGui::Checkbox("Loop", &state.fileLoop);
    ImGui::TextDisabled("Formats: .cf32/.fc32 (float32 IQ), .ci16/.sc16 (int16 IQ), .wav (PCM16)");
    ImGui::EndDisabled();
  }

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
