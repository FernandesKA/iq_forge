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
} // namespace

void AppState::updateDisplays() {
  if (deviceManager.pollHealth()) {
    log("Device disconnected (lost communication)");
  }

  SampleBuffer block;
  bool gotRx = false;
  while (rxRing.tryPop(block)) {
    gotRx = true;
    appendTrim(rxTimeDomain, block, kTimeDomainMaxSamples);
    processSpectrum(rxFft, block, rxSpectrumDb);
    if (rxRecording) {
      rxRecordBuffer.insert(rxRecordBuffer.end(), block.begin(), block.end());
    }
  }
  if (gotRx) pushWaterfallRow(rxWaterfallRows, rxSpectrumDb, kWaterfallMaxRows);

  bool gotTx = false;
  while (txPreviewRing.tryPop(block)) {
    gotTx = true;
    appendTrim(txTimeDomain, block, kTimeDomainMaxSamples);
    processSpectrum(txFft, block, txSpectrumDb);
  }
  if (gotTx) pushWaterfallRow(txWaterfallRows, txSpectrumDb, kWaterfallMaxRows);
}

} // namespace iqforge
