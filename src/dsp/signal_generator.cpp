#include "signal_generator.h"

#include <cmath>

namespace iqforge {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;

double wrapPhase(double phase) {
  phase = std::fmod(phase, kTwoPi);
  if (phase < 0.0) phase += kTwoPi;
  return phase;
}
} // namespace

SignalGenerator::SignalGenerator(GeneratorConfig cfg) : cfg_(std::move(cfg)) {
  multiTonePhases_.assign(cfg_.multiToneFreqsHz.size(), 0.0);
}

void SignalGenerator::setConfig(const GeneratorConfig& cfg) {
  std::lock_guard<std::mutex> lock(cfgMutex_);
  cfg_ = cfg;
  multiTonePhases_.assign(cfg_.multiToneFreqsHz.size(), 0.0);
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
    if (multiTonePhases_.size() != cfg.multiToneFreqsHz.size()) {
      multiTonePhases_.assign(cfg.multiToneFreqsHz.size(), 0.0);
    }
  }

  switch (cfg.type) {
    case WaveformType::Tone: generateTone(out, count, cfg); break;
    case WaveformType::MultiTone: generateMultiTone(out, count, cfg); break;
    case WaveformType::Chirp: generateChirp(out, count, cfg); break;
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
  const double bandwidth = cfg.chirpEndFreqHz - cfg.chirpStartFreqHz;
  const double k = bandwidth / duration; // Hz/sec sweep rate

  double t = chirpTime_;
  double phase = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double tMod = std::fmod(t, duration);
    const double instFreq = cfg.chirpStartFreqHz + k * tMod;
    phase = wrapPhase(phase + kTwoPi * instFreq * dt);
    out[i] = Sample(cfg.amplitude * std::cos(phase), cfg.amplitude * std::sin(phase));
    t += dt;
  }
  chirpTime_ = std::fmod(t, duration);
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
