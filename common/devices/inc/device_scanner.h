#pragma once

#include <string>
#include <vector>

namespace iqforge {

struct ScannedDevice {
  std::string description; // human-readable label to show in the UI
  std::string uri;          // value to place into DeviceConfig::uri
};

// Scans for PlutoSDR contexts reachable via USB and over the network.
// Network discovery uses libiio's "ip" backend, which resolves mDNS/Avahi
// advertisements (the ad9361 iiod service broadcasts as "iiod on <name>",
// _iio._tcp) — the same mechanism SDR++ and similar tools use to find a
// Pluto plugged in over Ethernet rather than USB. Blocks for up to a couple
// of seconds while the network is probed; call from a UI button, not every
// frame.
std::vector<ScannedDevice> scanPlutoDevices();

// Lists HackRF devices currently attached via USB (no network discovery —
// HackRF has no Ethernet interface).
std::vector<ScannedDevice> scanHackrfDevices();

} // namespace iqforge
