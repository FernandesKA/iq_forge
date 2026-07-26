#include "app_state.h"

#include <algorithm>
#include <chrono>

namespace iqforge {

namespace {
void appendTrim(std::vector<Sample>& buf, const SampleBuffer& block, size_t maxLen) {
  buf.insert(buf.end(), block.begin(), block.end());
  if (buf.size() > maxLen) {
    buf.erase(buf.begin(), buf.begin() + static_cast<long>(buf.size() - maxLen));
  }
}

void pushWaterfallRow(std::deque<WaterfallRow>& rows, const std::vector<float>& row, int maxRows) {
  rows.push_back(WaterfallRow{row, std::chrono::steady_clock::now()});
  while (static_cast<int>(rows.size()) > maxRows) rows.pop_front();
}

void processSpectrum(FftProcessor& fft, const SampleBuffer& block, std::vector<float>& outDb) {
  size_t n = fft.fftSize();
  if (block.size() >= n) {
    fft.process(block.data() + (block.size() - n), n, outDb);
  } else {
    fft.process(block.data(), block.size(), outDb);
  }
}

// Samples generated per frame for the idle TX preview below -- arbitrary,
// just enough to give the spectrum/time-domain views a decent-sized window;
// unrelated to any hardware TX buffer size since nothing is actually being
// streamed to a device here.
constexpr size_t kGeneratorPreviewSamples = 4096;
} // namespace

void AppState::updateDisplays() {
  if (deviceManager.pollHealth()) {
    log("Device disconnected (lost communication)");
  }

  SampleBuffer block;
  bool gotRx = false;
  while (rxRing.tryPop(block)) {
    // Recording is independent of the freeze below -- pausing the display
    // shouldn't silently pause a capture the user explicitly started.
    if (rxRecording) {
      rxRecordBuffer.insert(rxRecordBuffer.end(), block.begin(), block.end());
    }
    if (rxFrozen) continue;
    gotRx = true;
    appendTrim(rxTimeDomain, block, kTimeDomainMaxSamples);
    processSpectrum(rxFft, block, rxSpectrumDb);
  }
  if (gotRx) pushWaterfallRow(rxWaterfallRows, rxSpectrumDb, kWaterfallMaxRows);

  bool gotTx = false;
  if (!txFrozen) {
    while (txPreviewRing.tryPop(block)) {
      gotTx = true;
      appendTrim(txTimeDomain, block, kTimeDomainMaxSamples);
      processSpectrum(txFft, block, txSpectrumDb);
    }

    // Without an active TX, neither the generator nor a loaded file source
    // otherwise ever runs, so the signal being configured is invisible until
    // the user actually starts transmitting -- preview it directly here
    // instead, using the very same source instance startTx() would hand to
    // the device (safe: isTxActive() and the device's own thread-join on
    // stop ensure this and the real TX thread never call generate() at once).
    if (!gotTx && !isTxActive()) {
      block.resize(kGeneratorPreviewSamples);
      size_t got = 0;
      if (txSourceMode == 0) {
        got = generator->generate(block.data(), block.size());
      } else if (fileSource) {
        got = fileSource->generate(block.data(), block.size());
      }
      if (got > 0) {
        block.resize(got);
        gotTx = true;
        appendTrim(txTimeDomain, block, kTimeDomainMaxSamples);
        processSpectrum(txFft, block, txSpectrumDb);
      }
    }
  }

  if (gotTx) pushWaterfallRow(txWaterfallRows, txSpectrumDb, kWaterfallMaxRows);
}

void AppState::setFftSize(int newSize) {
  if (newSize == fftSize) return;
  fftSize = newSize;
  txFft.setFftSize(static_cast<size_t>(newSize));
  rxFft.setFftSize(static_cast<size_t>(newSize));
  txWaterfallRows.clear();
  rxWaterfallRows.clear();
}

} // namespace iqforge
