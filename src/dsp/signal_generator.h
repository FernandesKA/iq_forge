#pragma once

#include <mutex>
#include <random>
#include <vector>

#include "../core/sample_source.h"

namespace iqforge {

enum class WaveformType {
  Tone,
  MultiTone,
  Chirp,
  Pulse,
  Barker,
  Noise,
  Ramp,
};

// All Barker sequences, excluding variants obtainable only by negation or
// reversal. Lengths 2 and 4 each have two distinct sequences.
enum class BarkerCode {
  B2PlusMinus,
  B2PlusPlus,
  B3,
  B4PlusPlusMinusPlus,
  B4PlusPlusPlusMinus,
  B5,
  B7,
  B11,
  B13,
};

struct GeneratorConfig {
  WaveformType type = WaveformType::Tone;
  double sampleRateHz = 1e6;

  // Tone
  double toneFreqHz = 100e3;

  // MultiTone
  std::vector<double> multiToneFreqsHz = {50e3, 150e3, 300e3};

  // Chirp: linear sweep of chirpDeviationHz total bandwidth, centered on 0 Hz
  // baseband (i.e. from -deviation/2 to +deviation/2), over durationSec, then
  // repeats (sawtooth sweep). A negative deviation produces a down-chirp.
  double chirpDeviationHz = 800e3;
  double chirpDurationSec = 1e-3;

  // Barker: a continuously repeated BPSK chip sequence.
  BarkerCode barkerCode = BarkerCode::B13;
  double barkerChipRateHz = 100e3;

  // Pulse: a rectangular constant-amplitude (real) carrier gated on for
  // pulseDurationSec out of every pulsePeriodSec, then repeats. The same two
  // fields also drive the envelope below, so pulse timing is configured in
  // one place regardless of how it's applied.
  double pulseDurationSec = 10e-6;
  double pulsePeriodSec = 100e-6;

  // Envelope: gates any *other* waveform type on/off using the pulse timing
  // above, e.g. turning a chirp into a pulsed LFM radar signal. Ignored for
  // WaveformType::Pulse, which always applies its own gating.
  bool envelopeEnabled = false;

  float amplitude = 0.7f; // 0..1, leaves headroom to avoid clipping downstream
};

// Produces synthetic IQ test signals. Safe to reconfigure from another
// thread while generate() is being called from the TX thread.
class SignalGenerator : public ISampleSource {
 public:
  explicit SignalGenerator(GeneratorConfig cfg = {});

  void setConfig(const GeneratorConfig& cfg);
  GeneratorConfig config() const;

  size_t generate(Sample* out, size_t count) override;

 private:
  void generateTone(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateMultiTone(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateChirp(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generatePulse(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateBarker(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateNoise(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateRamp(Sample* out, size_t count, const GeneratorConfig& cfg);
  void applyEnvelope(Sample* out, size_t count, const GeneratorConfig& cfg);

  mutable std::mutex cfgMutex_;
  GeneratorConfig cfg_;

  double tonePhase_ = 0.0;
  std::vector<double> multiTonePhases_;
  double chirpTime_ = 0.0;
  double envelopeTime_ = 0.0;
  size_t barkerChipIndex_ = 0;
  double barkerChipPhase_ = 0.0;
  float rampValue_ = -1.0f;

  std::mt19937 rng_{std::random_device{}()};
  std::normal_distribution<float> noiseDist_{0.0f, 1.0f};
};

} // namespace iqforge
