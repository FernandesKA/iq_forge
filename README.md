# IQ Forge

A desktop GUI application for transmitting and receiving IQ signals with SDR hardware.

## Features

### Device support
- **PlutoSDR** and **HackRF** support, with automatic USB/network discovery (mDNS/Avahi) for PlutoSDR.
- Configurable sample rate, center frequency, bandwidth, TX/RX gain.
- Live connection health checks — the UI reflects when a device is unplugged.

### Signal generation (TX)
- Built-in waveform generator with multiple signal types:
  - Single tone
  - Multi-tone
  - Chirp (linear FM sweep, up or down)
  - Rectangular pulse
  - Barker-coded BPSK sequences (all standard codes, lengths 2–13)
  - Noise
  - Ramp
- Optional pulse envelope, allowing any waveform (e.g. a chirp) to be gated on/off to form a pulsed radar-like signal.
- Transmit from a loaded IQ file instead of the generator, with optional looping.
- Live preview of the transmitted signal (time domain + spectrum).

### Receiving (RX)
- Live time-domain and spectrum view of received samples.
- Scrolling waterfall/spectrogram display.
- Record incoming IQ samples to disk on demand.

### IQ file I/O
- Load and save IQ files in common formats:
  - Raw interleaved float32 (`.cf32`/`.fc32`)
  - Raw interleaved int16 (`.ci16`/`.sc16`)
  - PCM16 stereo WAV (`.wav`)
- Built-in file browser popup for picking files without typing paths by hand.

### Visualization
- Configurable FFT-based spectrum analyzer (window type, averaging).
- Time-domain plot.
- Waterfall plot with adjustable history depth.
- Zoom controls on plots.

## Building

```sh
cmake -B build
cmake --build build
```

Requires GLFW, OpenGL, libiio, fftw3f, and libhackrf development packages.

## Running tests

```sh
cmake --build build --target test
```
