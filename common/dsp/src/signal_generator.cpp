#include "signal_generator.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace iqforge {

namespace {
constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr double kPi = kTwoPi / 2.0;

// Gain of the envelope shape at tMod within an active [0, duration) window,
// 0 outside it. u is the normalized position across the window (0..1).
double envelopeGain(double tMod, double duration, EnvelopeShape shape) {
  if (duration <= 0.0 || tMod < 0.0 || tMod >= duration) return 0.0;
  const double u = tMod / duration;
  switch (shape) {
    case EnvelopeShape::Rectangular:
      return 1.0;
    case EnvelopeShape::Hann:
      return 0.5 - 0.5 * std::cos(kTwoPi * u);
    case EnvelopeShape::Sinc: {
      // 3 sidelobes on each side; x is 0 at the window center and +-3 at its
      // edges, where sin(x) naturally reaches zero -- no discontinuity.
      constexpr double kSideLobes = 3.0;
      const double x = (u - 0.5) * 2.0 * kSideLobes;
      return x == 0.0 ? 1.0 : std::sin(kPi * x) / (kPi * x);
    }
    case EnvelopeShape::Gaussian: {
      // sigma chosen so the window edges sit at +-3 sigma (~0.011 gain).
      constexpr double kSigma = 1.0 / 6.0;
      const double x = (u - 0.5) / kSigma;
      return std::exp(-0.5 * x * x);
    }
  }
  return 1.0;
}

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
  cfg.pulsePeriodSec = cfg.pulsePeriodSec > 0.0 ? cfg.pulsePeriodSec : 1e-6;
  cfg.pulseDurationSec = std::clamp(cfg.pulseDurationSec, 0.0, cfg.pulsePeriodSec);
  // QPSK packs 2 bits/symbol, so the symbol rate (which must stay below
  // Nyquist for the RRC filter to make sense) is half the bit rate --
  // letting the bit rate itself run up to 2x sample rate in that mode.
  const double maxBitRateHz = cfg.prbsQpskEnabled ? 2.0 * maxChipRateHz : maxChipRateHz;
  cfg.prbsBitRateHz = std::clamp(cfg.prbsBitRateHz, 0.0, maxBitRateHz);
  cfg.prbsRrcRolloff = std::clamp(cfg.prbsRrcRolloff, 0.0f, 1.0f);
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

// order/tap of the standard maximal-length polynomial x^order + x^tap + 1
// used by test equipment for each named PRBS sequence.
struct PrbsSpec {
  int order;
  int tap;
};

PrbsSpec prbsSpec(PrbsPolynomial p) {
  switch (p) {
    case PrbsPolynomial::Prbs7: return {7, 6};
    case PrbsPolynomial::Prbs9: return {9, 5};
    case PrbsPolynomial::Prbs11: return {11, 9};
    case PrbsPolynomial::Prbs15: return {15, 14};
    case PrbsPolynomial::Prbs23: return {23, 18};
    case PrbsPolynomial::Prbs31: return {31, 28};
  }
  return {15, 14};
}

// One Galois-LFSR step for the trinomial x^order + x^tap + 1: shifts right,
// XOR-ing in the tap mask whenever the bit shifted out was 1. Register stays
// within [0, 2^order) on its own -- both XOR'd bits (order-1 and
// order-1-tap) fall inside that range, so no masking is needed. Returns the
// bit shifted out.
int lfsrStep(uint32_t& reg, int order, int tap) {
  const uint32_t outBit = reg & 1u;
  reg >>= 1;
  if (outBit) {
    reg ^= (1u << (order - 1));
    reg ^= (1u << (order - 1 - tap));
  }
  return static_cast<int>(outBit);
}

// Root-raised-cosine impulse response shape at normalized time tau = t/Tsym,
// peak-normalized so h(0) == 1 (the overall gain is controlled separately by
// GeneratorConfig::amplitude, so absolute filter energy doesn't matter here).
// beta == 0 degenerates to an ideal (brick-wall) Nyquist sinc pulse.
double rrcShape(double tau, double beta) {
  if (beta <= 1e-6) {
    return tau == 0.0 ? 1.0 : std::sin(kPi * tau) / (kPi * tau);
  }
  if (std::abs(tau) < 1e-9) {
    return 1.0; // peak-normalized: true value here is 1 - beta + 4*beta/pi
  }
  const double denomArg = 4.0 * beta * tau;
  if (std::abs(std::abs(denomArg) - 1.0) < 1e-8) {
    // Removable singularity at tau = +-1/(4*beta); use the closed-form limit.
    const double c = kPi / (4.0 * beta);
    const double peak = 1.0 - beta + 4.0 * beta / kPi;
    const double v = (beta / std::sqrt(2.0)) * ((1.0 + 2.0 / kPi) * std::sin(c) + (1.0 - 2.0 / kPi) * std::cos(c));
    return v / peak;
  }
  const double peak = 1.0 - beta + 4.0 * beta / kPi;
  const double num = std::sin(kPi * tau * (1.0 - beta)) + denomArg * std::cos(kPi * tau * (1.0 + beta));
  const double den = kPi * tau * (1.0 - denomArg * denomArg);
  return (num / den) / peak;
}

// Symbols within +-kRrcHalfSpanSymbols of the current one are summed to
// evaluate the pulse-shaping filter at each output sample -- wide enough to
// capture the RRC tail down to a small fraction of its peak even at low
// roll-off, cheap enough (2*span+1 taps) to do per sample.
constexpr int kRrcHalfSpanSymbols = 6;
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

  // The raw-bit and QPSK paths consume PRBS bits at different rates and
  // can't share mid-sequence LFSR/window state, so switching polynomial or
  // mode restarts the sequence from its fixed all-ones seed.
  if (cfg.type == WaveformType::Prbs &&
      (!prbsInitialized_ || prbsActivePolynomial_ != cfg.prbsPolynomial || prbsQpskActive_ != cfg.prbsQpskEnabled)) {
    resetPrbsState(cfg);
  }

  switch (cfg.type) {
    case WaveformType::Tone: generateTone(out, count, cfg); break;
    case WaveformType::MultiTone: generateMultiTone(out, count, cfg); break;
    case WaveformType::Chirp: generateChirp(out, count, cfg); break;
    case WaveformType::Pulse: generatePulse(out, count, cfg); break;
    case WaveformType::Barker: generateBarker(out, count, cfg); break;
    case WaveformType::Noise: generateNoise(out, count, cfg); break;
    case WaveformType::Ramp: generateRamp(out, count, cfg); break;
    case WaveformType::Prbs: generatePrbs(out, count, cfg); break;
  }

  // Pulse always gates itself; any other waveform can opt into the same
  // shaped envelope to turn it into a pulsed signal (e.g. a pulsed chirp
  // for radar-style testing).
  if (cfg.type == WaveformType::Pulse || cfg.envelopeEnabled) {
    applyEnvelope(out, count, cfg);
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

void SignalGenerator::generatePulse(Sample* out, size_t count, const GeneratorConfig& cfg) {
  // The gating/shaping itself is applied uniformly afterwards by
  // applyEnvelope(); the "fill" for a bare pulse is just a constant (real)
  // carrier at 0 Hz baseband.
  std::fill(out, out + count, Sample(cfg.amplitude, 0.0f));
}

void SignalGenerator::applyEnvelope(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const double dt = 1.0 / cfg.sampleRateHz;
  const double period = cfg.pulsePeriodSec > 0.0 ? cfg.pulsePeriodSec : 1e-6;
  const double duration = std::clamp(cfg.pulseDurationSec, 0.0, period);

  double t = envelopeTime_;
  for (size_t i = 0; i < count; ++i) {
    const double tMod = std::fmod(t, period);
    out[i] *= static_cast<float>(envelopeGain(tMod, duration, cfg.envelopeShape));
    t += dt;
  }
  envelopeTime_ = std::fmod(t, period);
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

void SignalGenerator::resetPrbsState(const GeneratorConfig& cfg) {
  const PrbsSpec spec = prbsSpec(cfg.prbsPolynomial);
  prbsOrder_ = spec.order;
  prbsTap_ = spec.tap;
  prbsReg_ = (prbsOrder_ >= 32) ? 0xFFFFFFFFu : ((1u << prbsOrder_) - 1u); // all-ones: standard nonzero seed
  prbsBitPhase_ = 0.0;
  prbsCurrentBit_ = nextPrbsBit();
  prbsSymbolPhase_ = 0.0;
  prbsSymbolWindow_.clear();
  if (cfg.prbsQpskEnabled) {
    for (int i = 0; i < 2 * kRrcHalfSpanSymbols + 1; ++i) {
      prbsSymbolWindow_.push_back(nextPrbsQpskSymbol());
    }
  }
  prbsActivePolynomial_ = cfg.prbsPolynomial;
  prbsQpskActive_ = cfg.prbsQpskEnabled;
  prbsInitialized_ = true;
}

int SignalGenerator::nextPrbsBit() { return lfsrStep(prbsReg_, prbsOrder_, prbsTap_); }

Sample SignalGenerator::nextPrbsQpskSymbol() {
  constexpr float kInvSqrt2 = 0.70710678118654752f;
  const int i = nextPrbsBit();
  const int q = nextPrbsBit();
  return Sample(i ? kInvSqrt2 : -kInvSqrt2, q ? kInvSqrt2 : -kInvSqrt2);
}

void SignalGenerator::generatePrbs(Sample* out, size_t count, const GeneratorConfig& cfg) {
  if (cfg.prbsQpskEnabled) {
    generatePrbsQpsk(out, count, cfg);
  } else {
    generatePrbsBpsk(out, count, cfg);
  }
}

void SignalGenerator::generatePrbsBpsk(Sample* out, size_t count, const GeneratorConfig& cfg) {
  if (cfg.sampleRateHz <= 0.0 || cfg.prbsBitRateHz <= 0.0) {
    std::fill(out, out + count, Sample(0.0f, 0.0f));
    return;
  }
  const double bitsPerSample = cfg.prbsBitRateHz / cfg.sampleRateHz;
  double bitPhase = prbsBitPhase_;
  int bit = prbsCurrentBit_;
  for (size_t i = 0; i < count; ++i) {
    out[i] = Sample(bit ? cfg.amplitude : -cfg.amplitude, 0.0f);
    bitPhase += bitsPerSample;
    while (bitPhase >= 1.0) {
      bitPhase -= 1.0;
      bit = nextPrbsBit();
    }
  }
  prbsBitPhase_ = bitPhase;
  prbsCurrentBit_ = bit;
}

void SignalGenerator::generatePrbsQpsk(Sample* out, size_t count, const GeneratorConfig& cfg) {
  const double symbolRateHz = cfg.prbsBitRateHz / 2.0; // QPSK: 2 bits/symbol
  if (cfg.sampleRateHz <= 0.0 || symbolRateHz <= 0.0 || prbsSymbolWindow_.size() != static_cast<size_t>(2 * kRrcHalfSpanSymbols + 1)) {
    std::fill(out, out + count, Sample(0.0f, 0.0f));
    return;
  }
  const double symbolStep = symbolRateHz / cfg.sampleRateHz; // symbols per output sample
  const double beta = cfg.prbsRrcRolloff;

  double phase = prbsSymbolPhase_;
  for (size_t i = 0; i < count; ++i) {
    Sample acc(0.0f, 0.0f);
    for (int j = -kRrcHalfSpanSymbols; j <= kRrcHalfSpanSymbols; ++j) {
      const double tau = j + phase; // time offset to that symbol, in symbol periods
      const float h = static_cast<float>(rrcShape(tau, beta));
      acc += prbsSymbolWindow_[kRrcHalfSpanSymbols + j] * h;
    }
    out[i] = acc * cfg.amplitude;
    phase += symbolStep;
    if (phase >= 1.0) {
      phase -= 1.0;
      prbsSymbolWindow_.pop_front();
      prbsSymbolWindow_.push_back(nextPrbsQpskSymbol());
    }
  }
  prbsSymbolPhase_ = phase;
}

} // namespace iqforge
