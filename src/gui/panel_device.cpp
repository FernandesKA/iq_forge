#include "panel_device.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace iqforge {

namespace {
constexpr double kZero = 0.0;
constexpr double kMinAtten = -89.75;
constexpr double kHackrfTxMax = 47.0;
constexpr double kHackrfRxMax = 102.0;
constexpr double kPlutoRxMax = 77.0;

const char* kUriHint(DeviceKind kind) {
  switch (kind) {
    case DeviceKind::PlutoSDR: return "usb: | usb:1.5.5 | ip:192.168.2.1 | ip:pluto.local (empty = usb:)";
    case DeviceKind::HackRF: return "serial number (empty = first device found)";
  }
  return "";
}
} // namespace

void drawDevicePanel(AppState& state) {
  ImGui::Begin("Device");

  bool connected = state.deviceManager.isConnected();
  ImGui::BeginDisabled(connected);
  {
    int kind = static_cast<int>(state.selectedKind);
    if (ImGui::RadioButton("PlutoSDR", kind == 0)) state.selectedKind = DeviceKind::PlutoSDR;
    ImGui::SameLine();
    if (ImGui::RadioButton("HackRF", kind == 1)) state.selectedKind = DeviceKind::HackRF;

    ImGui::InputText("URI / Serial", state.uriBuffer, sizeof(state.uriBuffer));
    ImGui::TextDisabled("%s", kUriHint(state.selectedKind));

    if (ImGui::Button("Scan")) {
      state.scanResults = state.selectedKind == DeviceKind::PlutoSDR ? scanPlutoDevices() : scanHackrfDevices();
      state.log("Scan found " + std::to_string(state.scanResults.size()) + " device(s)");
    }
    ImGui::SameLine();
    ImGui::TextDisabled(state.selectedKind == DeviceKind::PlutoSDR
                             ? "Probes USB and the network (mDNS), like SDR++'s device list"
                             : "Lists HackRF units currently attached via USB");

    for (const auto& d : state.scanResults) {
      ImGui::PushID(d.uri.c_str());
      if (ImGui::Selectable(d.description.c_str())) {
        std::snprintf(state.uriBuffer, sizeof(state.uriBuffer), "%s", d.uri.c_str());
      }
      ImGui::PopID();
    }
  }
  ImGui::EndDisabled();

  ImGui::Separator();

  bool changed = false;
  changed |= ImGui::InputDouble("Sample rate (Hz)", &state.sampleRateHz, 0.0, 0.0, "%.0f");
  changed |= ImGui::InputDouble("Center freq (Hz)", &state.centerFreqHz, 0.0, 0.0, "%.0f");
  changed |= ImGui::InputDouble("Bandwidth (Hz)", &state.bandwidthHz, 0.0, 0.0, "%.0f");

  bool txGainChanged = ImGui::SliderScalar(
      state.selectedKind == DeviceKind::HackRF ? "TX VGA gain (dB)" : "TX attenuation (dB)",
      ImGuiDataType_Double, &state.txGainDb,
      state.selectedKind == DeviceKind::HackRF ? &kZero : &kMinAtten,
      state.selectedKind == DeviceKind::HackRF ? &kHackrfTxMax : &kZero, "%.2f");
  bool rxGainChanged = ImGui::SliderScalar(
      "RX gain (dB)", ImGuiDataType_Double, &state.rxGainDb, &kZero,
      state.selectedKind == DeviceKind::HackRF ? &kHackrfRxMax : &kPlutoRxMax, "%.2f");

  ImGui::Separator();

  if (!connected) {
    if (ImGui::Button("Connect")) {
      DeviceConfig cfg;
      cfg.kind = state.selectedKind;
      cfg.uri = state.uriBuffer;
      cfg.sampleRateHz = state.sampleRateHz;
      cfg.centerFreqHz = state.centerFreqHz;
      cfg.bandwidthHz = state.bandwidthHz;
      cfg.txGainDb = state.txGainDb;
      cfg.rxGainDb = state.rxGainDb;
      if (state.deviceManager.connect(cfg, state.connectError)) {
        state.log("Connected to " + state.deviceManager.device()->name());
        if (!state.connectError.empty()) {
          state.log("Warning: " + state.connectError);
          state.connectError.clear(); // logged, not a connect failure -- don't show as one after disconnect
        }
      } else {
        state.log("Connect failed: " + state.connectError);
      }
    }
  } else {
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "Connected: %s", state.deviceManager.device()->name().c_str());
    ImGui::SameLine();
    if (ImGui::Button("Disconnect")) {
      state.deviceManager.disconnect();
      state.log("Disconnected");
    }

    if (connected && changed) {
      IDevice* dev = state.deviceManager.device();
      if (!dev->setSampleRate(state.sampleRateHz)) state.log("Sample rate rejected by device");
      if (!dev->setFrequency(state.centerFreqHz)) state.log("Center frequency rejected by device");
      if (!dev->setBandwidth(state.bandwidthHz)) state.log("Bandwidth rejected by device");
    }
    if (connected && txGainChanged) state.deviceManager.device()->setTxGain(state.txGainDb);
    if (connected && rxGainChanged) state.deviceManager.device()->setRxGain(state.rxGainDb);
  }

  if (!state.connectError.empty() && !connected) {
    ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", state.connectError.c_str());
  }

  ImGui::End();
}

} // namespace iqforge
