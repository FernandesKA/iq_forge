#pragma once

#include <fftw3.h>

#include <vector>

#include "../core/sample_types.h"

namespace iqforge {

enum class WindowType { Rectangular, Hann, Hamming };

struct SpectrumConfig {
  size_t fftSize = 4096;
  WindowType window = WindowType::Hann;
  // Exponential moving average factor applied to the dB spectrum, in
  // [0, 1]. 1.0 = no averaging (latest frame only), smaller = smoother/slower.
  float averagingAlpha = 0.3f;
};

// Windowed, averaged magnitude spectrum of IQ data using FFTW (single
// precision). Not thread-safe; each producer of spectra should own one
// instance.
class FftProcessor {
 public:
  explicit FftProcessor(SpectrumConfig cfg = {});
  ~FftProcessor();

  FftProcessor(const FftProcessor&) = delete;
  FftProcessor& operator=(const FftProcessor&) = delete;

  // Processes exactly fftSize() samples from `in` (only the first fftSize()
  // are used if more are given) and writes the fftshifted dB-magnitude
  // spectrum into outDb (resized to fftSize()): outDb[0] is the most
  // negative frequency (-Fs/2), outDb[fftSize()/2] is DC.
  void process(const Sample* in, size_t n, std::vector<float>& outDb);

  size_t fftSize() const { return cfg_.fftSize; }
  void resetAveraging() { avgInit_ = false; }

 private:
  SpectrumConfig cfg_;
  std::vector<float> window_;
  fftwf_complex* fftIn_;
  fftwf_complex* fftOut_;
  fftwf_plan plan_;
  std::vector<float> avgDb_;
  bool avgInit_ = false;
};

} // namespace iqforge
