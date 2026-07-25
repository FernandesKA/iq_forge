#pragma once

#include <chrono>
#include <memory>
#include <string>

#include "i_device.h"

namespace iqforge {

// Owns the single currently-active device instance (Pluto or HackRF). The
// GUI's device panel talks to this instead of constructing backends itself.
class DeviceManager {
 public:
  bool connect(const DeviceConfig& cfg, std::string& errorOut);
  void disconnect();

  IDevice* device() { return device_.get(); }
  const IDevice* device() const { return device_.get(); }
  bool isConnected() const { return device_ != nullptr && device_->isOpen(); }

  // Periodically pings the connected device's hardware (throttled to about
  // once a second) and disconnects it if the ping fails. isOpen() alone
  // never notices a device that physically disappeared -- only close()
  // clears it -- so without this, "Connected" stays shown forever after an
  // unplug. Call once per GUI frame; returns true the frame it actually
  // detects and acts on a lost connection (so the caller can log it).
  bool pollHealth();

 private:
  std::unique_ptr<IDevice> device_;
  std::chrono::steady_clock::time_point lastHealthCheck_{};
};

} // namespace iqforge
