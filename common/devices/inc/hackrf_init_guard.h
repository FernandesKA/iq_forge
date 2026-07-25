#pragma once

#include <libhackrf/hackrf.h>

#include <cstdlib>
#include <mutex>

namespace iqforge {

// libhackrf must be hackrf_init()'d before use and hackrf_exit()'d once,
// after every device is closed. This app only ever has one device open (and
// one scan in flight) at a time, so a single process-wide init torn down at
// exit is sufficient. Shared between HackRFDevice and the device scanner.
inline void ensureHackrfInitialized() {
  static std::once_flag once;
  std::call_once(once, [] {
    hackrf_init();
    std::atexit([] { hackrf_exit(); });
  });
}

} // namespace iqforge
