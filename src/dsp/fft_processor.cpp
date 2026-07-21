#include "fft_processor.h"

#include <algorithm>
#include <cmath>

namespace iqforge {

namespace {
constexpr float kMinLinear = 1e-12f; // floor before log10 to avoid -inf

std::vector<float> buildWindow(WindowType type, size_t n) {
  std::vector<float> w(n, 1.0f);
  if (type == WindowType::Rectangular || n < 2) return w;
  constexpr double kTwoPi = 6.283185307179586476925286766559;
  for (size_t i = 0; i < n; ++i) {
    double phase = kTwoPi * static_cast<double>(i) / static_cast<double>(n - 1);
    if (type == WindowType::Hann) {
      w[i] = static_cast<float>(0.5 - 0.5 * std::cos(phase));
    } else if (type == WindowType::Hamming) {
      w[i] = static_cast<float>(0.54 - 0.46 * std::cos(phase));
    }
  }
  return w;
}
} // namespace

FftProcessor::FftProcessor(SpectrumConfig cfg) : cfg_(cfg) {
  window_ = buildWindow(cfg_.window, cfg_.fftSize);
  fftIn_ = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * cfg_.fftSize));
  fftOut_ = static_cast<fftwf_complex*>(fftwf_malloc(sizeof(fftwf_complex) * cfg_.fftSize));
  plan_ = fftwf_plan_dft_1d(static_cast<int>(cfg_.fftSize), fftIn_, fftOut_, FFTW_FORWARD, FFTW_MEASURE);
  avgDb_.assign(cfg_.fftSize, -160.0f);
}

FftProcessor::~FftProcessor() {
  fftwf_destroy_plan(plan_);
  fftwf_free(fftIn_);
  fftwf_free(fftOut_);
}

void FftProcessor::process(const Sample* in, size_t n, std::vector<float>& outDb) {
  const size_t fftSize = cfg_.fftSize;
  const size_t usable = std::min(n, fftSize);

  for (size_t i = 0; i < usable; ++i) {
    float w = window_[i];
    fftIn_[i][0] = in[i].real() * w;
    fftIn_[i][1] = in[i].imag() * w;
  }
  for (size_t i = usable; i < fftSize; ++i) {
    fftIn_[i][0] = 0.0f;
    fftIn_[i][1] = 0.0f;
  }

  fftwf_execute(plan_);

  outDb.resize(fftSize);
  const float normalize = 1.0f / static_cast<float>(fftSize);
  const size_t half = fftSize / 2;
  for (size_t k = 0; k < fftSize; ++k) {
    // fftshift: bin k maps to output index (k + half) mod fftSize, so index 0
    // of outDb is the most negative frequency and the center is DC.
    size_t shifted = (k + half) % fftSize;
    float re = fftOut_[k][0] * normalize;
    float im = fftOut_[k][1] * normalize;
    float mag2 = re * re + im * im;
    float db = 10.0f * std::log10(std::max(mag2, kMinLinear));
    outDb[shifted] = db;
  }

  if (!avgInit_ || avgDb_.size() != fftSize) {
    avgDb_ = outDb;
    avgInit_ = true;
  } else {
    const float a = std::clamp(cfg_.averagingAlpha, 0.0f, 1.0f);
    for (size_t i = 0; i < fftSize; ++i) {
      avgDb_[i] = a * outDb[i] + (1.0f - a) * avgDb_[i];
    }
  }
  outDb = avgDb_;
}

} // namespace iqforge
