#include "panel_help.h"

#include <imgui.h>

#include <initializer_list>

namespace iqforge {

namespace {
void bulletWrapped(const char* text) {
  ImGui::Bullet();
  ImGui::SameLine();
  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted(text);
  ImGui::PopTextWrapPos();
}

void section(const char* title, std::initializer_list<const char*> items) {
  if (ImGui::CollapsingHeader(title)) {
    for (const char* item : items) bulletWrapped(item);
  }
}
} // namespace

void drawHelpPanel(AppState&) {
  ImGui::Begin("Help");

  ImGui::PushTextWrapPos(0.0f);
  ImGui::TextUnformatted("Point-by-point notes on what each panel and control does. Click a section to expand it.");
  ImGui::PopTextWrapPos();
  ImGui::Separator();

  section("Device", {
      "PlutoSDR or HackRF -- pick before Connect; switching resets the gain fields to a safe default for that "
      "device.",
      "URI / Serial: PlutoSDR accepts usb:, usb:1.5.5, ip:192.168.2.1, ip:pluto.local, or empty for the first "
      "usb: device found; HackRF takes a serial number, or empty for the first device found.",
      "TX1/TX2 and RX1/RX2 (PlutoSDR only): which physical chain to use. TX2/RX2 need 2T2R firmware -- Connect "
      "fails with an error if selected on a single-channel unit.",
      "Scan: lists PlutoSDR units over USB and the network (mDNS), or HackRF units over USB. Click a result to "
      "fill in the URI/Serial field.",
      "Sample rate / Center freq / Bandwidth: apply live to a connected device as soon as they're edited.",
      "TX attenuation / TX VGA gain: PlutoSDR exposes attenuation (0 = max power, more negative = less power); "
      "HackRF exposes a positive VGA gain instead.",
      "RX gain mode (PlutoSDR only): Manual, AGC slow, or AGC fast. HackRF has no hardware AGC, so its RX gain "
      "is always manual.",
      "FFT size: shared by TX and RX spectrum/waterfall. Larger means finer frequency resolution but slower and "
      "more smeared in time; changing it clears the waterfall history.",
  });

  section("Control panel", {
      "One \"Control\" window whose content follows whichever main-area tab (TX, RX, or Signal Viewer) is "
      "currently selected -- switch tabs up top to switch what's shown here instead of hunting for a separate "
      "control tab.",
  });

  section("Control -- TX", {
      "Signal generator or IQ file: choose the TX source. Both preview their spectrum/waveform in the TX plot "
      "window even before Start TX is pressed.",
      "Waveform types: Tone (single carrier), Multi-tone (several simultaneous tones), Chirp/sweep (linear "
      "frequency sweep, sawtooth-repeating -- a negative deviation gives a down-chirp), Pulse (gated carrier), "
      "Barker (repeating BPSK Barker code, useful for correlation/timing tests), Noise (white Gaussian), Ramp "
      "(linear sawtooth amplitude), PRBS (repeating pseudorandom bit sequence from a standard LFSR polynomial).",
      "Pulse envelope: any non-Pulse waveform can optionally be gated the same way Pulse always is, e.g. turning "
      "a chirp into a pulsed LFM signal. Shapes: Rectangular (hard on/off), Sinc, Gaussian, Hann -- the last "
      "three taper at the edges to narrow sidelobes compared to a rectangle.",
      "PRBS QPSK + RRC shaping: off transmits raw bipolar chips (one +-amplitude sample per bit, like Barker); "
      "on maps bit pairs to QPSK symbols pulse-shaped by a root-raised-cosine filter -- e.g. for feeding a known "
      "bit pattern into an FPGA/demodulator under test.",
      "IQ file: Load reads (and, if enabled, resamples) the file on a background thread with a progress bar, so "
      "a large file or a large resample ratio doesn't freeze the GUI. Loop repeats the file; Resample on load "
      "converts it from the sample rate you specify to sampleRate x coefficient.",
      "SigMF files (.sigmf-data/.sigmf-meta) recover their sample rate and center frequency automatically -- "
      "\"Apply to device\" retunes the connected device to that recovered frequency (never automatic, since "
      "silently retuning hardware from a file load would be surprising).",
      "Start TX / Stop TX: only enabled once a device is connected; in file mode the file must be loaded first.",
  });

  section("Control -- RX", {
      "Start RX / Stop RX: only enabled once a device is connected.",
      "Record to buffer: while checked, every received block is appended to an in-memory buffer, independent of "
      "whether the RX plot display is frozen.",
      "Save format: SigMF (default) stores sample rate, center frequency, and annotations alongside the "
      "samples; Raw CF32 stores only the interleaved float32 samples.",
      "Mark now: flags the current recording position as a SigMF annotation, saved together with the recording.",
      "Save & clear: writes the buffered recording to disk and empties the buffer.",
  });

  section("Control -- Signal Viewer", {
      "Loads, previews, resamples, and saves IQ files without needing a device connected at all -- separate "
      "from TX's own IQ file source mode.",
      "Load works the same way as TX's IQ file Load: background thread, progress bar, optional resample-on-load, "
      "SigMF sample-rate/center-frequency recovery.",
      "Save writes exactly what's currently loaded (already resampled, if that was enabled) as SigMF or raw "
      "CF32 -- resample on load first if you want the saved file at a different rate.",
  });

  section("Plots -- TX / RX / Signal Viewer", {
      "Each shows Spectrum, Waterfall, I/Q, Phase, and Instantaneous frequency, independently toggleable; "
      "TX/RX show live device data (or an idle preview when nothing is actively streaming), Signal Viewer shows "
      "whatever file is currently loaded.",
      "Freeze: pauses just the display -- TX/RX/recording keep running underneath, so a signal can be inspected "
      "without it scrolling away.",
      "Ctrl+click a plot to place the selected marker (spectrum: M1..M4 with a Peak search; time-domain: M1..M4 "
      "shared across I/Q, Phase, and Inst. freq). Any marker can show a delta against another as a reference.",
      "Shift+drag a plot to measure a range: RMS/peak/crest factor/DC offset/clipping for a time-domain "
      "selection, or the equivalent readout for a spectrum band.",
      "Mouse wheel zooms the X axis, Shift+wheel zooms the Y axis; the H+/H-/V+/V- buttons and \"Fit signal\" do "
      "the same without needing the mouse over the plot.",
      "Trigger (time-domain plots): like an oscilloscope trigger -- redraws a periodic signal at a fixed edge "
      "crossing each frame instead of letting its phase drift/scroll.",
  });

  section("SpectrumViewer (wideband sweep)", {
      "For scanning a frequency range wider than the device can capture in one instantaneous acquisition.",
      "Add one or more bands (Start/End, or the equivalent Center/Span), select one, then Start sweep: the "
      "device retunes step by step (step size = the current Sample rate) across the range, and the composite "
      "spectrum fills in as each step completes.",
      "The sweep takes over RX -- Stop sweep only stops RX again if the sweep itself started it; if RX was "
      "already running, it's left alone.",
  });

  section("Settings", {
      "Save settings on exit / restore on startup: when enabled, the current device/TX/RX/Signal Viewer "
      "configuration is saved on close and restored on the next launch.",
      "Main area tabs: which of TX, RX, Signal Viewer, and SpectrumViewer show up as tabs in the main area -- "
      "unchecking one just hides it, it doesn't stop anything running underneath.",
      "Presets: save the entire current configuration under a name, then reload or delete it later -- "
      "independent of the auto-save toggle above.",
  });

  section("IQ file formats", {
      ".cf32/.fc32: raw interleaved float32 I,Q,I,Q,... (GNU Radio \"complex64\" convention). No sample rate is "
      "stored in the file -- you supply it when loading.",
      ".ci16/.sc16: raw interleaved int16 I,Q,I,Q,..., scaled to +-1. Also carries no sample rate.",
      ".wav: canonical PCM16 stereo WAV, left channel = I, right channel = Q.",
      ".sigmf-data + .sigmf-meta (SigMF, sigmf.org): a pair of files sharing a base name -- the data file holds "
      "the same raw samples as Cf32/Ci16, the meta file is JSON carrying sample rate, center frequency, "
      "recording hardware, and annotations. IQ Forge always writes cf32_le data.",
  });

  ImGui::End();
}

} // namespace iqforge
