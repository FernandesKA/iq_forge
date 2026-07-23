#include "panel_visualization.h"

#include <imgui.h>

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

void drawVisualizationWindow(const char* windowTitle, VisualizationTabState& tab,
                              const std::vector<Sample>& timeDomain, const std::vector<float>& spectrumDb,
                              const std::deque<std::vector<float>>& waterfallRows, double sampleRateHz) {
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
    plotSpectrum("##spectrum", spectrumDb, sampleRateHz, tab.spectrumView);
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

    if (tab.timeDomainCursor.active) {
      ImGui::Text("Cursor: sample %d", tab.timeDomainCursor.index);
      ImGui::SameLine();
      if (ImGui::Button("Clear cursor")) tab.timeDomainCursor.active = false;
    } else {
      ImGui::TextDisabled("Ctrl+click a plot below to select a sample");
    }

    if (tab.showIQ) {
      ImGui::Text("I/Q");
      ImGui::PushID("iq");
      plotIQLines("##iq", triggeredData, triggeredCount, tab.iqView, resetFromTrigger, tab.timeDomainXLink,
                  tab.timeDomainCursor);
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
}
} // namespace

void drawTxVisualizationPanel(AppState& state) {
  static VisualizationTabState tab;
  drawVisualizationWindow("TX", tab, state.txTimeDomain, state.txSpectrumDb, state.txWaterfallRows,
                           state.sampleRateHz);
}

void drawRxVisualizationPanel(AppState& state) {
  static VisualizationTabState tab;
  drawVisualizationWindow("RX", tab, state.rxTimeDomain, state.rxSpectrumDb, state.rxWaterfallRows,
                           state.sampleRateHz);
}

} // namespace iqforge
