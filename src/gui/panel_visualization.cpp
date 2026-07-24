#include "panel_visualization.h"

#include <imgui.h>

#include <cmath>

#include "plot_format.h"
#include "plot_instfreq.h"
#include "plot_phase.h"
#include "plot_spectrum.h"
#include "plot_time_domain.h"
#include "plot_trigger.h"
#include "plot_waterfall.h"

namespace iqforge {

namespace {
struct VisualizationTabState {
  bool showSpectrum = true;
  bool showWaterfall = true;
  bool showIQ = true;
  bool showPhase = true;
  bool showInstFreq = true;

  SpectrumViewState spectrumView;
  AxisZoomState waterfallZoom;
  // I/Q, Phase, and Instantaneous Frequency all derive from the same
  // triggered sample window, so they share one trigger (and one set of
  // trigger controls) instead of each independently re-deriving it. They
  // also share one X-axis zoom/pan link and one sample-selection cursor, so
  // zooming or Ctrl+clicking a sample in any one of them is reflected in
  // the other two.
  TriggerState trigger;
  TimeDomainViewState iqView;
  PhaseViewState phaseView;
  InstFreqViewState instFreqView;
  SharedXAxisLink timeDomainXLink;
  SampleCursorState timeDomainCursor;
};

// Reported back to the caller (which owns AppState/the device) when a
// spectrum marker's "-> Center freq" button was clicked, since this
// function only ever sees raw fields, not AppState itself.
struct VisualizationRequest {
  bool retuneRequested = false;
  double retuneToHz = 0.0;
};

void drawCursorReadout(SampleCursorState& cursor, const Sample* data, size_t count, double sampleRateHz) {
  bool any = cursor.active[0] || cursor.active[1];
  for (int slot = 0; slot < SampleCursorState::kCount; ++slot) {
    if (!cursor.active[slot]) continue;
    ImGui::Text("Cursor %c: sample %d", slot == 0 ? 'A' : 'B', cursor.index[slot]);
    ImGui::SameLine();
    ImGui::PushID(slot);
    if (ImGui::Button("Clear")) cursor.active[slot] = false;
    ImGui::PopID();
  }
  if (!any) {
    ImGui::TextDisabled("Ctrl+click: cursor A, Ctrl+Shift+click: cursor B (on any plot below)");
    return;
  }

  bool bothInRange = cursor.active[0] && cursor.active[1] && static_cast<size_t>(cursor.index[0]) < count &&
                      static_cast<size_t>(cursor.index[1]) < count;
  if (bothInRange && sampleRateHz > 0.0) {
    int ia = cursor.index[0], ib = cursor.index[1];
    double dtSamples = std::abs(ib - ia);
    double dt = dtSamples / sampleRateHz;
    double freq = dt > 0.0 ? 1.0 / dt : 0.0;
    float ampA = std::abs(data[ia]);
    float ampB = std::abs(data[ib]);
    ImGui::Text("dt (period): %s   1/dt (freq): %s   dAmplitude: %.4g", formatSeconds(dt).c_str(),
                formatHz(freq).c_str(), ampB - ampA);
  }
}

VisualizationRequest drawVisualizationWindow(const char* windowTitle, VisualizationTabState& tab,
                                              const std::vector<Sample>& timeDomain,
                                              const std::vector<float>& spectrumDb,
                                              const std::deque<WaterfallRow>& waterfallRows, double sampleRateHz,
                                              double centerFreqHz) {
  VisualizationRequest request;
  ImGui::Begin(windowTitle);

  ImGui::Checkbox("Spectrum", &tab.showSpectrum);
  ImGui::SameLine();
  ImGui::Checkbox("Waterfall", &tab.showWaterfall);
  ImGui::SameLine();
  ImGui::Checkbox("I/Q", &tab.showIQ);
  ImGui::SameLine();
  ImGui::Checkbox("Phase", &tab.showPhase);
  ImGui::SameLine();
  ImGui::Checkbox("Inst. freq", &tab.showInstFreq);
  ImGui::Separator();

  // Each section gets its own ID scope: "Fit signal"/"H+"/"H-"/"V+"/"V-"
  // are drawn once per section, and without this they'd collide (same
  // labels, same window) once more than one section is visible at a time.
  if (tab.showSpectrum) {
    ImGui::SeparatorText("Spectrum");
    ImGui::PushID("spectrum");
    plotSpectrum("##spectrum", spectrumDb, sampleRateHz, centerFreqHz, timeDomain, tab.spectrumView);
    if (tab.spectrumView.centerFreqRequested) {
      tab.spectrumView.centerFreqRequested = false;
      request.retuneRequested = true;
      request.retuneToHz = tab.spectrumView.requestedCenterFreqHz;
    }
    ImGui::PopID();
  }

  if (tab.showWaterfall) {
    ImGui::SeparatorText("Waterfall");
    ImGui::PushID("waterfall");
    plotWaterfall("##waterfall", waterfallRows, tab.waterfallZoom);
    ImGui::PopID();
  }

  if (tab.showIQ || tab.showPhase || tab.showInstFreq) {
    ImGui::SeparatorText("Time domain");
    ImGui::PushID("timedomain");
    bool resetFromTrigger = drawTriggerControls(tab.trigger);
    auto [triggeredData, triggeredCount] = applyTrigger(timeDomain, tab.trigger);

    drawCursorReadout(tab.timeDomainCursor, triggeredData, triggeredCount, sampleRateHz);

    if (tab.showIQ) {
      ImGui::Text("I/Q");
      ImGui::PushID("iq");
      plotIQLines("##iq", triggeredData, triggeredCount, tab.iqView, resetFromTrigger, tab.timeDomainXLink,
                  tab.timeDomainCursor, tab.trigger);
      ImGui::PopID();
    }
    if (tab.showPhase) {
      ImGui::Text("Phase");
      ImGui::PushID("phase");
      plotPhaseLine("##phase", triggeredData, triggeredCount, tab.phaseView, resetFromTrigger, tab.timeDomainXLink,
                    tab.timeDomainCursor);
      ImGui::PopID();
    }
    if (tab.showInstFreq) {
      ImGui::Text("Instantaneous frequency");
      ImGui::PushID("instfreq");
      plotInstFreqLine("##instfreq", triggeredData, triggeredCount, sampleRateHz, tab.instFreqView, resetFromTrigger,
                        tab.timeDomainXLink, tab.timeDomainCursor);
      ImGui::PopID();
    }
    ImGui::PopID();
  }

  ImGui::End();
  return request;
}

// Applies a spectrum marker's "-> Center freq" request: retunes the shared
// device state and, if a device is connected, the hardware itself -- same
// pattern panel_device.cpp uses when its own frequency field changes.
void applyCenterFreqRetune(AppState& state, double hz) {
  state.centerFreqHz = hz;
  IDevice* dev = state.deviceManager.device();
  if (!dev) return;
  if (!dev->setFrequency(hz)) {
    state.log("Marker -> center freq: rejected by device");
  } else {
    state.log("Marker -> center freq: tuned to " + formatHz(hz));
  }
}
} // namespace

void drawTxVisualizationPanel(AppState& state) {
  static VisualizationTabState tab;
  VisualizationRequest req = drawVisualizationWindow("TX", tab, state.txTimeDomain, state.txSpectrumDb,
                                                       state.txWaterfallRows, state.sampleRateHz, state.centerFreqHz);
  if (req.retuneRequested) applyCenterFreqRetune(state, req.retuneToHz);
}

void drawRxVisualizationPanel(AppState& state) {
  static VisualizationTabState tab;
  VisualizationRequest req = drawVisualizationWindow("RX", tab, state.rxTimeDomain, state.rxSpectrumDb,
                                                       state.rxWaterfallRows, state.sampleRateHz, state.centerFreqHz);
  if (req.retuneRequested) applyCenterFreqRetune(state, req.retuneToHz);
}

} // namespace iqforge
