#include "signal_generator.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace iqforge {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;

double wrapPhase(double phase) {
  phase = std::fmod(phase, kTwoPi);
  if (phase < 0.0) phase += kTwoPi;
  return phase;
}

GeneratorConfig clampBasebandFrequencies(GeneratorConfig cfg) {
  const double nyquistHz = std::abs(cfg.sampleRateHz) * 0.5;
  const auto clampFrequency = [nyquistHz](double hz) {
    return std::clamp(hz, -nyquistHz, nyquistHz);
  };

  cfg.toneFreqHz = clampFrequency(cfg.toneFreqHz);
  for (double& hz : cfg.multiToneFreqsHz) hz = clampFrequency(hz);
  // Deviation spans both sides of 0 Hz (+-deviation/2), so its magnitude may
  // be up to a full sample rate before either endpoint exceeds Nyquist.
  cfg.chirpDeviationHz = std::clamp(cfg.chirpDeviationHz, -std::abs(cfg.sampleRateHz), std::abs(cfg.sampleRateHz));
  const double maxChipRateHz = std::abs(cfg.sampleRateHz);
  cfg.barkerChipRateHz = std::clamp(cfg.barkerChipRateHz, 0.0, maxChipRateHz);
  return cfg;
}

const std::vector<int>& barkerChips(BarkerCode code) {
  static const std::vector<int> b2PlusMinus = {1, -1};
  static const std::vector<int> b2PlusPlus = {1, 1};
  static const std::vector<int> b3 = {1, 1, -1};
  static const std::vector<int> b4PlusPlusMinusPlus = {1, 1, -1, 1};
  static const std::vector<int> b4PlusPlusPlusMinus = {1, 1, 1, -1};
  static const std::vector<int> b5 = {1, 1, 1, -1, 1};
  static const std::vector<int> b7 = {1, 1, 1, -1, -1, 1, -1};
  static const std::vector<int> b11 = {1, 1, 1, -1, -1, -1, 1, -1, -1, 1, -1};
  static const std::vector<int> b13 = {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1};

  switch (code) {
    case BarkerCode::B2PlusMinus: return b2PlusMinus;
    case BarkerCode::B2PlusPlus: return b2PlusPlus;
    case BarkerCode::B3: return b3;
    case BarkerCode::B4PlusPlusMinusPlus: return b4PlusPlusMinusPlus;
    case BarkerCode::B4PlusPlusPlusMinus: return b4PlusPlusPlusMinus;
    case BarkerCode::B5: return b5;
    case BarkerCode::B7: return b7;
    case BarkerCode::B11: return b11;
    case BarkerCode::B13: return b13;
  }
  return b13;
}
} // namespace

SignalGenerator::SignalGenerator(GeneratorConfig cfg) : cfg_(clampBasebandFrequencies(std::move(cfg))) {
  multiTonePhases_.assign(cfg_.multiToneFreqsHz.size(), 0.0);
}

void SignalGenerator::setConfig(const GeneratorConfig& cfg) {
  std::lock_guard<std::mutex> lock(cfgMutex_);
  cfg_ = clampBasebandFrequencies(cfg);
}

GeneratorConfig SignalGenerator::config() const {
  std::lock_guard<std::mutex> lock(cfgMutex_);
  return cfg_;
}

size_t SignalGenerator::generate(Sample* out, size_t count) {
  GeneratorConfig cfg;
  {
    std::lock_guard<std::mutex> lock(cfgMutex_);
    cfg = cfg_;
  }
  // Phase state is owned exclusively by the thread calling generate().
  // setConfig() only swaps cfg_, so live GUI updates cannot race this resize.
  if (multiTonePhases_.size() != cfg.multiToneFreqsHz.size()) {
    multiTonePhases_.resize(cfg.multiToneFreqsHz.size(), 0.0);
  }

  switch (cfg.type) {
    case WaveformType::Tone: generateTone(out, count, cfg); break;
    case WaveformType::MultiTone: generateMultiTone(out, count, cfg); break;
    case WaveformType::Chirp: generateChirp(out, count, cfg); break;
    case WaveformType::Barker: generateBarker(out, count, cfg); break;
    case WaveformType::Noise: generateNoise(out, count, cfg); break;
    case WaveformType::Ramp: generateRamp(out, count, cfg); break;
  }
  return count;
}

void SignalGenerator::generateTone(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const double step = kTwoPi * cfg.toneFreqHz / cfg.sampleRateHz;
  double phase = tonePhase_;
  for (size_t i = 0; i < count; ++i) {
    out[i] = Sample(cfg.amplitude * std::cos(phase), cfg.amplitude * std::sin(phase));
    phase = wrapPhase(phase + step);
  }
  tonePhase_ = phase;
}

void SignalGenerator::generateMultiTone(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const size_t n = cfg.multiToneFreqsHz.size();
  for (size_t i = 0; i < count; ++i) out[i] = Sample(0.0f, 0.0f);
  if (n == 0) return;

  const float perToneAmp = cfg.amplitude / static_cast<float>(n);
  for (size_t t = 0; t < n; ++t) {
    const double step = kTwoPi * cfg.multiToneFreqsHz[t] / cfg.sampleRateHz;
    double phase = multiTonePhases_[t];
    for (size_t i = 0; i < count; ++i) {
      out[i] += Sample(perToneAmp * std::cos(phase), perToneAmp * std::sin(phase));
      phase = wrapPhase(phase + step);
    }
    multiTonePhases_[t] = phase;
  }
}

void SignalGenerator::generateChirp(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const double dt = 1.0 / cfg.sampleRateHz;
  const double duration = cfg.chirpDurationSec > 0.0 ? cfg.chirpDurationSec : 1e-3;
  const double startFreq = -cfg.chirpDeviationHz / 2.0;
  const double k = cfg.chirpDeviationHz / duration; // Hz/sec sweep rate

  double t = chirpTime_;
  double phase = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double tMod = std::fmod(t, duration);
    const double instFreq = startFreq + k * tMod;
    phase = wrapPhase(phase + kTwoPi * instFreq * dt);
    out[i] = Sample(cfg.amplitude * std::cos(phase), cfg.amplitude * std::sin(phase));
    t += dt;
  }
  chirpTime_ = std::fmod(t, duration);
}

void SignalGenerator::generateBarker(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const std::vector<int>& chips = barkerChips(cfg.barkerCode);
  if (cfg.sampleRateHz <= 0.0 || cfg.barkerChipRateHz <= 0.0 || chips.empty()) {
    std::fill(out, out + count, Sample(0.0f, 0.0f));
    return;
  }

  size_t chipIndex = barkerChipIndex_ % chips.size();
  double chipPhase = barkerChipPhase_;
  const double chipsPerSample = cfg.barkerChipRateHz / cfg.sampleRateHz;
  for (size_t i = 0; i < count; ++i) {
    out[i] = Sample(cfg.amplitude * static_cast<float>(chips[chipIndex]), 0.0f);
    chipPhase += chipsPerSample;
    while (chipPhase >= 1.0) {
      chipPhase -= 1.0;
      chipIndex = (chipIndex + 1) % chips.size();
    }
  }
  barkerChipIndex_ = chipIndex;
  barkerChipPhase_ = chipPhase;
}

void SignalGenerator::generateNoise(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const float scale = cfg.amplitude * 0.5f; // keep complex Gaussian noise within +-1 typically
  for (size_t i = 0; i < count; ++i) {
    out[i] = Sample(scale * noiseDist_(rng_), scale * noiseDist_(rng_));
  }
}

void SignalGenerator::generateRamp(Sample* out, size_t count, const GeneratorConfig& cfg) {
  float v = rampValue_;
  const float step = 2.0f / 1000.0f; // full -1..1 ramp every 1000 samples
  for (size_t i = 0; i < count; ++i) {
    out[i] = Sample(cfg.amplitude * v, 0.0f);
    v += step;
    if (v > 1.0f) v -= 2.0f;
  }
  rampValue_ = v;
}

} // namespace iqforge
