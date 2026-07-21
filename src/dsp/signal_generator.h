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
  Noise,
  Ramp,
};

struct GeneratorConfig {
  WaveformType type = WaveformType::Tone;
  double sampleRateHz = 1e6;

  // Tone
  double toneFreqHz = 100e3;

  // MultiTone
  std::vector<double> multiToneFreqsHz = {50e3, 150e3, 300e3};

  // Chirp: linear sweep from startFreqHz to endFreqHz over durationSec, then
  // repeats (sawtooth sweep).
  double chirpStartFreqHz = -400e3;
  double chirpEndFreqHz = 400e3;
  double chirpDurationSec = 1e-3;

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
  void generateNoise(Sample* out, size_t count, const GeneratorConfig& cfg);
  void generateRamp(Sample* out, size_t count, const GeneratorConfig& cfg);

  mutable std::mutex cfgMutex_;
  GeneratorConfig cfg_;

  double tonePhase_ = 0.0;
  std::vector<double> multiTonePhases_;
  double chirpTime_ = 0.0;
  float rampValue_ = -1.0f;

  std::mt19937 rng_{std::random_device{}()};
  std::normal_distribution<float> noiseDist_{0.0f, 1.0f};
};

} // namespace iqforge
