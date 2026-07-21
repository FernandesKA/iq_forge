#include "pluto_device.h"

#include <iio.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace iqforge {

namespace {
// AD9361 TX/RX streaming channels (cf-ad9361-dds-core-lpc / cf-ad9361-lpc)
// present already-full-scale 16-bit signed samples, so we treat them like
// plain 16-bit PCM — consistent with the Ci16 IQ file convention used
// elsewhere in this app.
constexpr float kHostToDevice = 32767.0f;
constexpr float kDeviceToHost = 1.0f / 32768.0f;

iio_channel* findPhyChannel(iio_device* phy, bool output) {
  return iio_device_find_channel(phy, "voltage0", output);
}
iio_channel* findLoChannel(iio_device* phy, bool tx) {
  // RX LO = altvoltage0, TX LO = altvoltage1 (both are output channels).
  return iio_device_find_channel(phy, tx ? "altvoltage1" : "altvoltage0", true);
}
} // namespace

PlutoDevice::PlutoDevice() = default;

PlutoDevice::~PlutoDevice() {
  close();
}

namespace {
// AD9361 rejects out-of-range attribute writes (e.g. a sample rate below
// its current minimum, which depends on clock/filter config and is *not*
// a fixed constant) with plain EINVAL and no other feedback. Silently
// ignoring that would leave the device running at some other rate than
// what the UI shows, so every write here is checked and reported.
void checkedWriteLL(iio_channel* chn, const char* attr, long long value, std::vector<std::string>& warnings) {
  if (iio_channel_attr_write_longlong(chn, attr, value) < 0) {
    warnings.push_back(std::string(attr) + "=" + std::to_string(value) + " rejected by device");
  }
}
} // namespace

bool PlutoDevice::configurePhy(std::string& errorOut) {
  iio_channel* rxPhy = findPhyChannel(phy_, false);
  iio_channel* txPhy = findPhyChannel(phy_, true);
  iio_channel* rxLo = findLoChannel(phy_, false);
  iio_channel* txLo = findLoChannel(phy_, true);
  if (!rxPhy || !txPhy || !rxLo || !txLo) {
    errorOut = "Could not find AD9361 phy channels";
    return false;
  }

  std::vector<std::string> warnings;

  iio_channel_attr_write(rxPhy, "rf_port_select", "A_BALANCED");
  iio_channel_attr_write(txPhy, "rf_port_select", "A");

  checkedWriteLL(rxPhy, "rf_bandwidth", static_cast<long long>(cfg_.bandwidthHz), warnings);
  checkedWriteLL(txPhy, "rf_bandwidth", static_cast<long long>(cfg_.bandwidthHz), warnings);
  checkedWriteLL(rxPhy, "sampling_frequency", static_cast<long long>(cfg_.sampleRateHz), warnings);
  checkedWriteLL(txPhy, "sampling_frequency", static_cast<long long>(cfg_.sampleRateHz), warnings);

  iio_channel_attr_write(rxPhy, "gain_control_mode", "manual");
  iio_channel_attr_write_double(rxPhy, "hardwaregain", cfg_.rxGainDb);
  iio_channel_attr_write_double(txPhy, "hardwaregain", cfg_.txGainDb);

  checkedWriteLL(rxLo, "frequency", static_cast<long long>(cfg_.centerFreqHz), warnings);
  checkedWriteLL(txLo, "frequency", static_cast<long long>(cfg_.centerFreqHz), warnings);

  if (!warnings.empty()) {
    errorOut = "Device rejected some settings (still connected, actual values may differ): ";
    for (size_t i = 0; i < warnings.size(); ++i) {
      if (i) errorOut += "; ";
      errorOut += warnings[i];
    }
  }
  return true;
}

bool PlutoDevice::open(const DeviceConfig& cfg, std::string& errorOut) {
  close();
  cfg_ = cfg;

  std::string uri = cfg.uri.empty() ? "usb:" : cfg.uri;
  ctx_ = iio_create_context_from_uri(uri.c_str());
  if (!ctx_) {
    errorOut = "Failed to connect to PlutoSDR at URI '" + uri + "'";
    return false;
  }

  phy_ = iio_context_find_device(ctx_, "ad9361-phy");
  rxDev_ = iio_context_find_device(ctx_, "cf-ad9361-lpc");
  txDev_ = iio_context_find_device(ctx_, "cf-ad9361-dds-core-lpc");
  if (!phy_ || !rxDev_ || !txDev_) {
    errorOut = "PlutoSDR context is missing expected ad9361 devices";
    close();
    return false;
  }

  if (!configurePhy(errorOut)) {
    close();
    return false;
  }

  rxChanI_ = iio_device_find_channel(rxDev_, "voltage0", false);
  rxChanQ_ = iio_device_find_channel(rxDev_, "voltage1", false);
  txChanI_ = iio_device_find_channel(txDev_, "voltage0", true);
  txChanQ_ = iio_device_find_channel(txDev_, "voltage1", true);
  if (!rxChanI_ || !rxChanQ_ || !txChanI_ || !txChanQ_) {
    errorOut = "PlutoSDR context is missing expected I/Q streaming channels";
    close();
    return false;
  }

  iio_channel_enable(rxChanI_);
  iio_channel_enable(rxChanQ_);
  iio_channel_enable(txChanI_);
  iio_channel_enable(txChanQ_);

  return true;
}

void PlutoDevice::close() {
  stopTx();
  stopRx();
  if (ctx_) {
    iio_context_destroy(ctx_);
    ctx_ = nullptr;
  }
  phy_ = txDev_ = rxDev_ = nullptr;
  txChanI_ = txChanQ_ = rxChanI_ = rxChanQ_ = nullptr;
}

bool PlutoDevice::startTx(std::shared_ptr<ISampleSource> source, std::string& errorOut) {
  if (!isOpen()) {
    errorOut = "Device not open";
    return false;
  }
  if (txRunning_.load()) return true;

  txBuf_ = iio_device_create_buffer(txDev_, kBufferSamples, false);
  if (!txBuf_) {
    errorOut = "Failed to create PlutoSDR TX buffer";
    return false;
  }

  txStopFlag_ = false;
  txRunning_ = true;
  txThread_ = std::thread(&PlutoDevice::txThreadFunc, this, std::move(source));
  return true;
}

void PlutoDevice::stopTx() {
  if (!txRunning_.exchange(false)) return;
  txStopFlag_ = true;
  if (txThread_.joinable()) txThread_.join();
  if (txBuf_) {
    iio_buffer_destroy(txBuf_);
    txBuf_ = nullptr;
  }
}

bool PlutoDevice::startRx(RxCallback callback, std::string& errorOut) {
  if (!isOpen()) {
    errorOut = "Device not open";
    return false;
  }
  if (rxRunning_.load()) return true;

  rxBuf_ = iio_device_create_buffer(rxDev_, kBufferSamples, false);
  if (!rxBuf_) {
    errorOut = "Failed to create PlutoSDR RX buffer";
    return false;
  }

  rxStopFlag_ = false;
  rxRunning_ = true;
  rxThread_ = std::thread(&PlutoDevice::rxThreadFunc, this, std::move(callback));
  return true;
}

void PlutoDevice::stopRx() {
  if (!rxRunning_.exchange(false)) return;
  rxStopFlag_ = true;
  if (rxThread_.joinable()) rxThread_.join();
  if (rxBuf_) {
    iio_buffer_destroy(rxBuf_);
    rxBuf_ = nullptr;
  }
}

void PlutoDevice::txThreadFunc(std::shared_ptr<ISampleSource> source) {
  std::vector<Sample> block(kBufferSamples);

  while (!txStopFlag_.load()) {
    size_t got = source->generate(block.data(), kBufferSamples);
    if (got == 0) break; // source exhausted (non-looping file finished)

    ptrdiff_t inc = iio_buffer_step(txBuf_);
    char* end = static_cast<char*>(iio_buffer_end(txBuf_));
    size_t i = 0;
    for (char* p = static_cast<char*>(iio_buffer_first(txBuf_, txChanI_)); p < end && i < got; p += inc, ++i) {
      int16_t* iq = reinterpret_cast<int16_t*>(p);
      iq[0] = static_cast<int16_t>(std::clamp(block[i].real(), -1.0f, 1.0f) * kHostToDevice);
      iq[1] = static_cast<int16_t>(std::clamp(block[i].imag(), -1.0f, 1.0f) * kHostToDevice);
    }

    ssize_t pushed = iio_buffer_push(txBuf_);
    if (pushed < 0) break; // device gone / fatal streaming error
  }

  txRunning_ = false;
}

void PlutoDevice::rxThreadFunc(RxCallback callback) {
  std::vector<Sample> block;
  block.reserve(kBufferSamples);

  while (!rxStopFlag_.load()) {
    ssize_t nbytes = iio_buffer_refill(rxBuf_);
    if (nbytes < 0) break; // device gone / fatal streaming error

    block.clear();
    ptrdiff_t inc = iio_buffer_step(rxBuf_);
    char* end = static_cast<char*>(iio_buffer_end(rxBuf_));
    for (char* p = static_cast<char*>(iio_buffer_first(rxBuf_, rxChanI_)); p < end; p += inc) {
      const int16_t* iq = reinterpret_cast<const int16_t*>(p);
      block.emplace_back(iq[0] * kDeviceToHost, iq[1] * kDeviceToHost);
    }
    if (!block.empty()) callback(block.data(), block.size());
  }

  rxRunning_ = false;
}

bool PlutoDevice::setFrequency(double hz) {
  if (!phy_) return false;
  cfg_.centerFreqHz = hz;
  iio_channel* rxLo = findLoChannel(phy_, false);
  iio_channel* txLo = findLoChannel(phy_, true);
  if (!rxLo || !txLo) return false;
  bool ok = iio_channel_attr_write_longlong(rxLo, "frequency", static_cast<long long>(hz)) == 0;
  ok &= iio_channel_attr_write_longlong(txLo, "frequency", static_cast<long long>(hz)) == 0;
  return ok;
}

bool PlutoDevice::setSampleRate(double sps) {
  if (!phy_) return false;
  cfg_.sampleRateHz = sps;
  iio_channel* rxPhy = findPhyChannel(phy_, false);
  iio_channel* txPhy = findPhyChannel(phy_, true);
  if (!rxPhy || !txPhy) return false;
  bool ok = iio_channel_attr_write_longlong(rxPhy, "sampling_frequency", static_cast<long long>(sps)) == 0;
  ok &= iio_channel_attr_write_longlong(txPhy, "sampling_frequency", static_cast<long long>(sps)) == 0;
  return ok;
}

bool PlutoDevice::setBandwidth(double hz) {
  if (!phy_) return false;
  cfg_.bandwidthHz = hz;
  iio_channel* rxPhy = findPhyChannel(phy_, false);
  iio_channel* txPhy = findPhyChannel(phy_, true);
  if (!rxPhy || !txPhy) return false;
  bool ok = iio_channel_attr_write_longlong(rxPhy, "rf_bandwidth", static_cast<long long>(hz)) == 0;
  ok &= iio_channel_attr_write_longlong(txPhy, "rf_bandwidth", static_cast<long long>(hz)) == 0;
  return ok;
}

bool PlutoDevice::setTxGain(double db) {
  if (!phy_) return false;
  cfg_.txGainDb = db;
  iio_channel* txPhy = findPhyChannel(phy_, true);
  if (!txPhy) return false;
  return iio_channel_attr_write_double(txPhy, "hardwaregain", db) == 0;
}

bool PlutoDevice::setRxGain(double db) {
  if (!phy_) return false;
  cfg_.rxGainDb = db;
  iio_channel* rxPhy = findPhyChannel(phy_, false);
  if (!rxPhy) return false;
  return iio_channel_attr_write_double(rxPhy, "hardwaregain", db) == 0;
}

} // namespace iqforge
