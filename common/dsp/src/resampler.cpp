#include "resampler.h"

#include <samplerate.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace iqforge {

namespace {
int converterFor(ResampleQuality q) {
  switch (q) {
    case ResampleQuality::Best: return SRC_SINC_BEST_QUALITY;
    case ResampleQuality::Medium: return SRC_SINC_MEDIUM_QUALITY;
    case ResampleQuality::Fastest: return SRC_SINC_FASTEST;
  }
  return SRC_SINC_BEST_QUALITY;
}

// Deletes the SRC_STATE on scope exit (including via an exception thrown
// out of src_process()) -- libsamplerate is a plain C API with no RAII of
// its own.
struct SrcStateGuard {
  SRC_STATE* state;
  ~SrcStateGuard() {
    if (state) src_delete(state);
  }
};

// Input frames fed to libsamplerate per src_process() call. Purely a
// progress-reporting granularity knob -- smaller means more frequent
// (cheaper than the actual resampling work per call, so this doesn't
// meaningfully add overhead) onProgress callbacks; libsamplerate's streaming
// API guarantees the same total output regardless of how the input is
// chunked, so this doesn't affect the result.
constexpr long kChunkFrames = 1 << 16;
} // namespace

SampleBuffer resampleIq(const SampleBuffer& input, double inputRateHz, double outputRateHz, ResampleQuality quality,
                         const std::function<void(float)>& onProgress) {
  if (input.empty()) return {};
  if (!(inputRateHz > 0.0) || !(outputRateHz > 0.0)) {
    throw std::runtime_error("Resample rates must be positive");
  }

  double ratio = outputRateHz / inputRateHz;
  if (!src_is_valid_ratio(ratio)) {
    throw std::runtime_error("Resample ratio is outside libsamplerate's supported range (~1:256..256:1)");
  }
  // Fast path: running a no-op ratio through the sinc filter would still
  // introduce filter-edge artifacts at the buffer boundaries for no benefit.
  if (std::abs(ratio - 1.0) < 1e-9) {
    if (onProgress) onProgress(1.0f);
    return input;
  }

  int err = 0;
  SRC_STATE* state = src_new(converterFor(quality), 2, &err);
  if (!state) {
    throw std::runtime_error(std::string("Resampling failed: ") + src_strerror(err));
  }
  SrcStateGuard guard{state};

  // std::complex<float> has the same object representation as float[2]
  // (real, imag) since C++11 -- the same layout libsamplerate expects for
  // 2-channel interleaved audio, and the same assumption plot_time_domain.cpp
  // already relies on when handing Sample* to ImPlot as interleaved floats.
  const float* inBase = reinterpret_cast<const float*>(input.data());
  const long totalInputFrames = static_cast<long>(input.size());

  // +1 for rounding headroom, same as the old single-shot call; grown below
  // if per-chunk rounding ever runs it close to full anyway, so this is a
  // starting estimate, not a hard cap.
  long outCapacity = static_cast<long>(std::ceil(static_cast<double>(input.size()) * ratio)) + 1;
  SampleBuffer output(static_cast<size_t>(outCapacity));
  float* outBase = reinterpret_cast<float*>(output.data());

  long inputPos = 0;
  long outputPos = 0;
  for (;;) {
    // Keep at least a full chunk's worth of output headroom before every
    // call, so a call is never starved of room to write into regardless of
    // how the +1 estimate above tracks the real ratio over many chunks.
    if (outCapacity - outputPos < kChunkFrames) {
      outCapacity += kChunkFrames * 2;
      output.resize(static_cast<size_t>(outCapacity));
      outBase = reinterpret_cast<float*>(output.data());
    }

    long remainingIn = totalInputFrames - inputPos;
    long feedFrames = std::min<long>(kChunkFrames, remainingIn);
    // True from the last real chunk of input onward (remainingIn only
    // shrinks), including the trailing flush-only calls once feedFrames hits
    // 0 -- src_process() needs end_of_input=1 held from here on to know it
    // should drain its internal filter delay rather than wait for more input.
    bool isEof = remainingIn <= kChunkFrames;

    SRC_DATA data{};
    data.data_in = inBase + inputPos * 2;
    data.input_frames = feedFrames;
    data.data_out = outBase + outputPos * 2;
    data.output_frames = outCapacity - outputPos;
    data.src_ratio = ratio;
    data.end_of_input = isEof ? 1 : 0;

    int perr = src_process(state, &data);
    if (perr != 0) {
      throw std::runtime_error(std::string("Resampling failed: ") + src_strerror(perr));
    }

    inputPos += data.input_frames_used;
    outputPos += data.output_frames_gen;

    if (onProgress) {
      onProgress(std::min(1.0f, static_cast<float>(inputPos) / static_cast<float>(totalInputFrames)));
    }

    // Nothing left to feed and this call's flush produced nothing new ->
    // the filter's internal delay has fully drained.
    if (feedFrames == 0 && data.output_frames_gen == 0) break;
  }

  output.resize(static_cast<size_t>(outputPos));
  if (onProgress) onProgress(1.0f);
  return output;
}

} // namespace iqforge
