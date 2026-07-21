#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "../core/ring_buffer.h"
#include "../core/sample_types.h"
#include "../core/tee_sample_source.h"
#include "../devices/device_manager.h"
#include "../devices/device_scanner.h"
#include "../dsp/fft_processor.h"
#include "../dsp/iq_file.h"
#include "../dsp/signal_generator.h"
#include "freq_input.h"

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
  FreqUnit chirpStartUnit = FreqUnit::kHz;
  FreqUnit chirpEndUnit = FreqUnit::kHz;
  FreqUnit barkerChipRateUnit = FreqUnit::kHz;
  char filePathBuffer[512] = "";
  bool fileLoop = true;
  std::string txError;
  RingBuffer<SampleBuffer> txPreviewRing{8};
  std::vector<Sample> txTimeDomain;
  std::vector<float> txSpectrumDb;
  FftProcessor txFft{SpectrumConfig{2048, WindowType::Hann, 0.3f}};

  bool isTxActive() const {
    return deviceManager.device() && deviceManager.device()->isTxRunning();
  }

  // --- RX ---
  std::string rxError;
  RingBuffer<SampleBuffer> rxRing{8};
  std::vector<Sample> rxTimeDomain;
  std::vector<float> rxSpectrumDb;
  FftProcessor rxFft{SpectrumConfig{2048, WindowType::Hann, 0.3f}};
  std::deque<std::vector<float>> waterfallRows;

  bool rxRecording = false;
  char rxRecordPathBuffer[512] = "recording.cf32";
  std::vector<Sample> rxRecordBuffer;

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
