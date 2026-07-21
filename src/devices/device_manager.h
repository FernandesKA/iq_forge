#pragma once

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

 private:
  std::unique_ptr<IDevice> device_;
};

} // namespace iqforge
