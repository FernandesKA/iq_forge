#pragma once

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/sample_types.h"

namespace iqforge {

enum class TriggerSource { I, Q, Magnitude };

// Oscilloscope-style trigger for a scrolling IQ buffer. Without it, a
// periodic signal's phase relative to the displayed window drifts every
// frame -- exactly like a scope with no trigger, where the waveform just
// runs across the screen. With it, each frame searches the buffer for the
// most recent edge crossing and always displays the same fixed-size window
// starting there, so a periodic signal redraws in the same place instead
// of scrolling.
struct TriggerState {
  bool enabled = true;
  bool risingEdge = true;
  TriggerSource source = TriggerSource::I;
  bool autoLevel = true;
  float manualLevel = 0.0f;
  int windowSamples = 2000;

  // Held from the last successful search, shown when the current buffer
  // doesn't (yet) contain a fresh crossing so the plot doesn't flicker.
  std::vector<Sample> heldWindow;
  bool hasHeld = false;
};

namespace detail {
inline float triggerSourceValue(const Sample& s, TriggerSource src) {
  switch (src) {
    case TriggerSource::I: return s.real();
    case TriggerSource::Q: return s.imag();
    case TriggerSource::Magnitude: return std::abs(s);
  }
  return s.real();
}
} // namespace detail

// Draws the trigger control row (enable, edge, source, level, window size).
// Returns true if a setting changed in a way that should force the plot to
// re-fit its axes (e.g. window size or the enabled state changed).
inline bool drawTriggerControls(TriggerState& trig) {
  bool resetView = false;

  bool wasEnabled = trig.enabled;
  ImGui::Checkbox("Trigger", &trig.enabled);
  resetView |= (trig.enabled != wasEnabled);

  ImGui::BeginDisabled(!trig.enabled);

  ImGui::SameLine();
  ImGui::SetNextItemWidth(90.0f);
  int edge = trig.risingEdge ? 0 : 1;
  const char* kEdgeNames[] = {"Rising", "Falling"};
  if (ImGui::Combo("##edge", &edge, kEdgeNames, IM_ARRAYSIZE(kEdgeNames))) {
    trig.risingEdge = (edge == 0);
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f);
  int source = static_cast<int>(trig.source);
  const char* kSourceNames[] = {"I", "Q", "Magnitude"};
  ImGui::Combo("##source", &source, kSourceNames, IM_ARRAYSIZE(kSourceNames));
  trig.source = static_cast<TriggerSource>(source);

  ImGui::SameLine();
  ImGui::Checkbox("Auto level", &trig.autoLevel);

  if (!trig.autoLevel) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Level", &trig.manualLevel, 0.001f, -2.0f, 2.0f, "%.4f");
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
  int prevWindow = trig.windowSamples;
  ImGui::SliderInt("Window (samples)", &trig.windowSamples, 64, 8192);
  resetView |= (trig.windowSamples != prevWindow);

  ImGui::EndDisabled();

  return resetView;
}

// Selects what to plot: the full rolling buffer when the trigger is off, or
// a phase-stable window found by searching backward for the most recent
// edge crossing of `trig.source` through `level`.
inline std::pair<const Sample*, size_t> applyTrigger(const std::vector<Sample>& data, TriggerState& trig) {
  if (!trig.enabled || data.empty()) {
    return {data.data(), data.size()};
  }

  size_t window = static_cast<size_t>(std::max(trig.windowSamples, 1));
  if (data.size() < window) {
    return {data.data(), data.size()};
  }

  float level = trig.manualLevel;
  if (trig.autoLevel) {
    float lo = 1e30f;
    float hi = -1e30f;
    for (const auto& s : data) {
      float v = detail::triggerSourceValue(s, trig.source);
      lo = std::min(lo, v);
      hi = std::max(hi, v);
    }
    level = (lo + hi) * 0.5f;
  }

  // Search from the latest index that still leaves a full window after it,
  // going backward, so we find the *most recent* usable crossing.
  size_t searchEnd = data.size() - window;
  for (long i = static_cast<long>(searchEnd); i >= 1; --i) {
    size_t idx = static_cast<size_t>(i);
    float prev = detail::triggerSourceValue(data[idx - 1], trig.source);
    float curr = detail::triggerSourceValue(data[idx], trig.source);
    bool crossed = trig.risingEdge ? (prev < level && curr >= level) : (prev > level && curr <= level);
    if (crossed) {
      const Sample* start = data.data() + idx;
      trig.heldWindow.assign(start, start + window);
      trig.hasHeld = true;
      return {start, window};
    }
  }

  if (trig.hasHeld && trig.heldWindow.size() == window) {
    return {trig.heldWindow.data(), trig.heldWindow.size()};
  }
  // No crossing found yet and nothing held (e.g. level/source just changed):
  // show the most recent window rather than an empty plot.
  return {data.data() + (data.size() - window), window};
}

} // namespace iqforge
