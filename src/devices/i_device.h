#pragma once

#include <functional>
#include <memory>
#include <string>

#include "../core/sample_source.h"

namespace iqforge {

enum class DeviceKind { PlutoSDR, HackRF };

struct DeviceConfig {
  DeviceKind kind = DeviceKind::PlutoSDR;

  // PlutoSDR: libiio context URI, e.g. "usb:" (first found), "usb:1.5.5",
  // "ip:192.168.2.1", "ip:pluto.local". Empty = auto ("usb:" then "ip:pluto.local").
  // HackRF: device serial number, empty = first device found.
  std::string uri;

  // 3 MSPS default: safely inside the AD9361's standard-firmware range
  // (roughly 2.083-61.44 MSPS without a custom FIR filter loaded; below
  // ~2.083 MSPS the device rejects the rate outright).
  double sampleRateHz = 3e6;
  double centerFreqHz = 915e6;
  double bandwidthHz = 2e6;

  // Pluto: TX attenuation in dB, 0 (max power) .. -89.75. HackRF: mapped to
  // txvga gain 0..47 dB.
  double txGainDb = -10.0;
  // Pluto: RX gain in dB, 0..77 (manual gain control). HackRF: mapped to
  // lna (0-40, 8dB steps) + vga (0-62, 2dB steps) gain.
  double rxGainDb = 30.0;
};

using RxCallback = std::function<void(const Sample* data, size_t count)>;

// Common interface for a TX/RX capable SDR device. Implementations own a
// background thread for TX (pulling from an ISampleSource) and/or RX
// (pushing captured blocks to a callback); the GUI thread only calls the
// control methods below and never touches hardware directly.
class IDevice {
 public:
  virtual ~IDevice() = default;

  virtual bool open(const DeviceConfig& cfg, std::string& errorOut) = 0;
  virtual void close() = 0;
  virtual bool isOpen() const = 0;

  // Round-trips a cheap query to the hardware to confirm it is still
  // reachable (e.g. still plugged in). isOpen() alone only reflects local
  // handle state, which stays "open" even after the device physically
  // disappears -- callers should poll this periodically and disconnect on
  // failure to keep the UI's connected state honest.
  virtual bool checkAlive() = 0;

  virtual bool startTx(std::shared_ptr<ISampleSource> source, std::string& errorOut) = 0;
  virtual void stopTx() = 0;
  virtual bool isTxRunning() const = 0;

  // callback is invoked from the device's RX thread — keep it cheap
  // (e.g. push into a RingBuffer) and never block on the GUI.
  virtual bool startRx(RxCallback callback, std::string& errorOut) = 0;
  virtual void stopRx() = 0;
  virtual bool isRxRunning() const = 0;

  virtual bool setFrequency(double hz) = 0;
  virtual bool setSampleRate(double sps) = 0;
  virtual bool setBandwidth(double hz) = 0;
  virtual bool setTxGain(double db) = 0;
  virtual bool setRxGain(double db) = 0;

  virtual std::string name() const = 0;
};

std::unique_ptr<IDevice> createDevice(DeviceKind kind);

} // namespace iqforge
