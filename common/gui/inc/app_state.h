#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "ring_buffer.h"
#include "sample_types.h"
#include "tee_sample_source.h"
#include "device_manager.h"
#include "device_scanner.h"
#include "fft_processor.h"
#include "iq_file.h"
#include "signal_generator.h"
#include "duration_input.h"
#include "freq_input.h"
#include "waterfall_row.h"

namespace iqforge {

// Everything the GUI panels read/write, gathered in one place so panel draw
// functions can just take an AppState& instead of threading a dozen
// references through. Owned by App, updated once per frame on the GUI
// thread; device I/O threads only ever touch the RingBuffers below.
struct AppState {
  static constexpr size_t kTimeDomainMaxSamples = 8192;
  static constexpr int kWaterfallMaxRows = 100;

  DeviceManager deviceManager;

  // --- Device panel ---
  DeviceKind selectedKind = DeviceKind::PlutoSDR;
  char uriBuffer[256] = "";
  double sampleRateHz = 3e6; // PlutoSDR's standard firmware rejects rates below ~2.083 MSPS
  double centerFreqHz = 915e6;
  double bandwidthHz = 2e6;
  FreqUnit sampleRateUnit = FreqUnit::MHz;
  FreqUnit centerFreqUnit = FreqUnit::MHz;
  FreqUnit bandwidthUnit = FreqUnit::MHz;
  double txGainDb = -89.75; // maximum PlutoSDR TX attenuation (safest startup level)
  double rxGainDb = 0.0;    // minimum RX gain
  std::string connectError;
  std::vector<ScannedDevice> scanResults;

  // --- TX ---
  int txSourceMode = 0; // 0 = generator, 1 = IQ file
  std::shared_ptr<SignalGenerator> generator = std::make_shared<SignalGenerator>();
  GeneratorConfig genConfig;
  FreqUnit toneFreqUnit = FreqUnit::kHz;
  FreqUnit multiToneUnit = FreqUnit::kHz; // shared by all entries in genConfig.multiToneFreqsHz
  FreqUnit chirpDeviationUnit = FreqUnit::kHz;
  TimeUnit chirpDurationUnit = TimeUnit::Ms;
  FreqUnit barkerChipRateUnit = FreqUnit::kHz;
  TimeUnit pulseDurationUnit = TimeUnit::Us;
  TimeUnit pulsePeriodUnit = TimeUnit::Us;
  char filePathBuffer[512] = "";
  bool fileLoop = true;
  // Raw formats (.cf32/.ci16) don't store their own sample rate, so the user
  // states what the file was recorded at. On Load, resampleIq() converts it
  // to fileSourceRateHz * fileResampleCoefficient (the resulting rate shown
  // in the UI) if resampling is enabled.
  double fileSourceRateHz = 3e6;
  FreqUnit fileSourceRateUnit = FreqUnit::MHz;
  bool fileResampleEnabled = false;
  double fileResampleCoefficient = 1.0;
  std::string fileLoadedPath; // path fileSource below was actually loaded from, "" if none/stale
  std::string fileLoadError;
  // Set by Load whenever fileLoadedPath was a SigMF Recording (.sigmf-data/
  // .sigmf-meta) -- carries the sample rate/frequency/annotations recovered
  // from the sidecar for display, since a bare .cf32/.ci16 has no way to
  // store any of that. fileSourceRateHz above is already auto-filled from
  // this at Load time; nullopt for a plain .cf32/.ci16/.wav file.
  std::optional<SigmfMeta> fileSigmfInfo;
  // Shared by the idle preview in updateDisplays() and "Start TX" -- Start
  // TX just hands this same instance to the device, so playback continues
  // from wherever the preview's read position left off instead of jumping
  // back to the start.
  std::shared_ptr<IQFileSource> fileSource;
  std::string txError;
  RingBuffer<SampleBuffer> txPreviewRing{8};
  std::vector<Sample> txTimeDomain;
  std::vector<float> txSpectrumDb;
  FftProcessor txFft{SpectrumConfig{2048, WindowType::Hann, 0.3f}};
  std::deque<WaterfallRow> txWaterfallRows;
  // Pauses updateDisplays()'s writes to the fields above -- TX itself (and,
  // for RX below, recording) keeps running untouched; only what's shown
  // stops changing, so a signal can be inspected/measured without it
  // scrolling out from under the cursor.
  bool txFrozen = false;

  bool isTxActive() const {
    return deviceManager.device() && deviceManager.device()->isTxRunning();
  }

  // --- RX ---
  std::string rxError;
  RingBuffer<SampleBuffer> rxRing{8};
  std::vector<Sample> rxTimeDomain;
  std::vector<float> rxSpectrumDb;
  FftProcessor rxFft{SpectrumConfig{2048, WindowType::Hann, 0.3f}};
  std::deque<WaterfallRow> rxWaterfallRows;
  // See txFrozen above. Recording (below) is unaffected by this -- it's
  // fed from the same drained blocks regardless of whether the display is
  // frozen, since pausing the view shouldn't silently pause a recording.
  bool rxFrozen = false;

  bool rxRecording = false;
  // SigMF is the default: unlike raw Cf32Raw, it records sample rate, center
  // frequency, recording hardware, and annotations alongside the samples
  // (see saveSigmf() in iq_file.h) instead of losing all of that on save.
  enum class RxSaveFormat { Sigmf, Cf32Raw };
  RxSaveFormat rxSaveFormat = RxSaveFormat::Sigmf;
  char rxRecordPathBuffer[512] = "recording.sigmf-data";
  char rxRecordDescriptionBuffer[256] = ""; // optional SigMF core:description
  std::vector<Sample> rxRecordBuffer;

  // Events the user flags while recording ("Mark now"), turned into SigMF
  // core:annotations on save. Sample offsets are relative to the start of
  // rxRecordBuffer, so they're only meaningful until it's cleared (Save &
  // clear resets both together).
  struct RxAnnotation {
    uint64_t sampleStart = 0;
    uint64_t sampleCount = 1;
    std::string label;
  };
  std::vector<RxAnnotation> rxRecordAnnotations;
  char rxAnnotationLabelBuffer[128] = "";

  bool isRxActive() const {
    return deviceManager.device() && deviceManager.device()->isRxRunning();
  }

  // --- Log ---
  std::mutex logMutex;
  std::deque<std::string> logMessages;
  void log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex);
    logMessages.push_back(msg);
    if (logMessages.size() > 500) logMessages.pop_front();
  }

  // Called once per frame on the GUI thread: drains ring buffers, updates
  // time-domain/spectrum/waterfall data.
  void updateDisplays();
};

} // namespace iqforge
