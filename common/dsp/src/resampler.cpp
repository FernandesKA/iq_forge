#include "resampler.h"

#include <samplerate.h>

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
} // namespace

SampleBuffer resampleIq(const SampleBuffer& input, double inputRateHz, double outputRateHz,
                         ResampleQuality quality) {
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
  if (std::abs(ratio - 1.0) < 1e-9) return input;

  // std::complex<float> has the same object representation as float[2]
  // (real, imag) since C++11 -- the same layout libsamplerate expects for
  // 2-channel interleaved audio, and the same assumption plot_time_domain.cpp
  // already relies on when handing Sample* to ImPlot as interleaved floats.
  const float* inFrames = reinterpret_cast<const float*>(input.data());

  // +1 for rounding headroom; src_simple() reports the frames it actually
  // wrote in output_frames_gen, which is what the result gets trimmed to.
  long outFrameCount = static_cast<long>(std::ceil(static_cast<double>(input.size()) * ratio)) + 1;

  SampleBuffer output(static_cast<size_t>(outFrameCount));

  SRC_DATA data{};
  data.data_in = inFrames;
  data.input_frames = static_cast<long>(input.size());
  data.data_out = reinterpret_cast<float*>(output.data());
  data.output_frames = outFrameCount;
  data.src_ratio = ratio;
  data.end_of_input = 1; // one-shot: the whole file, not a streaming chunk

  int err = src_simple(&data, converterFor(quality), 2);
  if (err != 0) {
    throw std::runtime_error(std::string("Resampling failed: ") + src_strerror(err));
  }

  output.resize(static_cast<size_t>(data.output_frames_gen));
  return output;
}

} // namespace iqforge
