#include <algorithm>
#include <cmath>
#include <vector>

#include "../src/dsp/fft_processor.h"
#include "../src/dsp/signal_generator.h"
#include "test_framework.h"

using namespace iqforge;

namespace {
size_t peakBin(const std::vector<float>& db) {
  return static_cast<size_t>(std::max_element(db.begin(), db.end()) - db.begin());
}

double wrapToPi(double phase) {
  constexpr double kPi = 3.14159265358979323846;
  while (phase > kPi) phase -= 2.0 * kPi;
  while (phase < -kPi) phase += 2.0 * kPi;
  return phase;
}

// Estimates instantaneous frequency (Hz) from the phase step between two
// adjacent IQ samples taken sampleRateHz apart.
double instFreqHz(const Sample& a, const Sample& b, double sampleRateHz) {
  constexpr double kTwoPi = 6.283185307179586476925286766559;
  double dPhase = wrapToPi(std::atan2(b.imag(), b.real()) - std::atan2(a.imag(), a.real()));
  return dPhase / kTwoPi * sampleRateHz;
}
} // namespace

void run_signal_generator_tests() {
  constexpr size_t fftSize = 4096;
  constexpr double sampleRate = 1e6;

  // Tone at Fs/8 should show up as a spectral peak near bin (fftSize/2 + fftSize/8),
  // since FftProcessor fftshifts so index fftSize/2 is DC.
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Tone;
    cfg.sampleRateHz = sampleRate;
    cfg.toneFreqHz = sampleRate / 8.0;
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);

    std::vector<Sample> buf(fftSize);
    CHECK(gen.generate(buf.data(), buf.size()) == fftSize);

    FftProcessor fft({fftSize, WindowType::Hann, 1.0f});
    std::vector<float> db;
    fft.process(buf.data(), buf.size(), db);

    size_t expected = fftSize / 2 + fftSize / 8;
    size_t got = peakBin(db);
    CHECK(got >= expected - 2 && got <= expected + 2);
  }

  // Ramp output must stay within [-amplitude, amplitude].
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Ramp;
    cfg.amplitude = 0.5f;
    SignalGenerator gen(cfg);
    std::vector<Sample> buf(5000);
    gen.generate(buf.data(), buf.size());
    for (const auto& s : buf) {
      CHECK(s.real() >= -0.5001f && s.real() <= 0.5001f);
      CHECK(s.imag() == 0.0f);
    }
  }

  // A running generator must use a newly supplied tone frequency on the
  // next block; the GUI relies on this for live TX tuning.
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Tone;
    cfg.sampleRateHz = sampleRate;
    cfg.toneFreqHz = sampleRate / 8.0;
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);

    std::vector<Sample> buf(fftSize);
    gen.generate(buf.data(), buf.size());

    cfg.toneFreqHz = sampleRate / 4.0;
    gen.setConfig(cfg);
    gen.generate(buf.data(), buf.size());

    FftProcessor fft({fftSize, WindowType::Hann, 1.0f});
    std::vector<float> db;
    fft.process(buf.data(), buf.size(), db);

    size_t expected = fftSize / 2 + fftSize / 4;
    size_t got = peakBin(db);
    CHECK(got >= expected - 2 && got <= expected + 2);
  }

  // Frequencies outside the complex-IQ Nyquist interval must be clamped
  // instead of silently aliasing to another RF frequency.
  {
    GeneratorConfig cfg;
    cfg.sampleRateHz = sampleRate;
    cfg.toneFreqHz = 100e6;
    cfg.multiToneFreqsHz = {-100e6, 100e6};
    cfg.chirpDeviationHz = 200e6;
    cfg.barkerChipRateHz = 100e6;

    SignalGenerator gen(cfg);
    GeneratorConfig actual = gen.config();
    CHECK(actual.toneFreqHz == sampleRate / 2.0);
    CHECK(actual.multiToneFreqsHz[0] == -sampleRate / 2.0);
    CHECK(actual.multiToneFreqsHz[1] == sampleRate / 2.0);
    CHECK(actual.chirpDeviationHz == sampleRate);
    CHECK(actual.barkerChipRateHz == sampleRate);

    cfg.toneFreqHz = -100e6;
    gen.setConfig(cfg);
    CHECK(gen.config().toneFreqHz == -sampleRate / 2.0);
  }

  // Every distinct Barker sequence must be emitted as repeating real BPSK
  // chips. At one sample per chip the generated samples equal the code.
  {
    struct BarkerCase {
      BarkerCode code;
      std::vector<int> chips;
    };
    const std::vector<BarkerCase> cases = {
        {BarkerCode::B2PlusMinus, {1, -1}},
        {BarkerCode::B2PlusPlus, {1, 1}},
        {BarkerCode::B3, {1, 1, -1}},
        {BarkerCode::B4PlusPlusMinusPlus, {1, 1, -1, 1}},
        {BarkerCode::B4PlusPlusPlusMinus, {1, 1, 1, -1}},
        {BarkerCode::B5, {1, 1, 1, -1, 1}},
        {BarkerCode::B7, {1, 1, 1, -1, -1, 1, -1}},
        {BarkerCode::B11, {1, 1, 1, -1, -1, -1, 1, -1, -1, 1, -1}},
        {BarkerCode::B13, {1, 1, 1, 1, 1, -1, -1, 1, 1, -1, 1, -1, 1}},
    };

    for (const BarkerCase& test : cases) {
      GeneratorConfig cfg;
      cfg.type = WaveformType::Barker;
      cfg.sampleRateHz = sampleRate;
      cfg.barkerChipRateHz = sampleRate;
      cfg.barkerCode = test.code;
      cfg.amplitude = 1.0f;
      SignalGenerator gen(cfg);

      std::vector<Sample> buf(test.chips.size() * 2);
      gen.generate(buf.data(), buf.size());
      for (size_t i = 0; i < buf.size(); ++i) {
        CHECK(buf[i].real() == static_cast<float>(test.chips[i % test.chips.size()]));
        CHECK(buf[i].imag() == 0.0f);
      }

      for (size_t lag = 1; lag < test.chips.size(); ++lag) {
        int correlation = 0;
        for (size_t i = 0; i + lag < test.chips.size(); ++i) {
          correlation += test.chips[i] * test.chips[i + lag];
        }
        CHECK(std::abs(correlation) <= 1);
      }
    }
  }

  // Noise should not be constant (basic sanity check it's actually random).
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Noise;
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);
    std::vector<Sample> buf(1000);
    gen.generate(buf.data(), buf.size());
    bool allSame = std::all_of(buf.begin(), buf.end(), [&](const Sample& s) { return s == buf.front(); });
    CHECK(!allSame);
  }

  // MultiTone with N tones should keep combined amplitude bounded near cfg.amplitude.
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::MultiTone;
    cfg.sampleRateHz = sampleRate;
    cfg.multiToneFreqsHz = {50e3, 150e3};
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);
    std::vector<Sample> buf(2000);
    gen.generate(buf.data(), buf.size());
    for (const auto& s : buf) {
      CHECK(std::abs(s) <= 1.01f);
    }
  }

  // Chirp (LFM) instantaneous frequency must ramp linearly across the
  // configured deviation (centered on 0 Hz) over one sweep, hold constant
  // amplitude, and restart (sawtooth) after chirpDurationSec elapses.
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Chirp;
    cfg.sampleRateHz = sampleRate;
    cfg.chirpDeviationHz = 400e3;
    const size_t sweepSamples = 4096;
    cfg.chirpDurationSec = static_cast<double>(sweepSamples) / sampleRate;
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);

    std::vector<Sample> buf(sweepSamples);
    gen.generate(buf.data(), buf.size());

    for (const auto& s : buf) {
      CHECK(std::abs(s) >= 0.99f && std::abs(s) <= 1.01f);
    }

    const double dt = 1.0 / sampleRate;
    const double startFreq = -cfg.chirpDeviationHz / 2.0;
    const double k = cfg.chirpDeviationHz / cfg.chirpDurationSec;
    const size_t checkIdx[] = {5, sweepSamples / 4, sweepSamples / 2, sweepSamples * 3 / 4 - 1};
    for (size_t i : checkIdx) {
      double expected = startFreq + k * (static_cast<double>(i) * dt);
      double actual = instFreqHz(buf[i], buf[i + 1], sampleRate);
      CHECK(std::abs(actual - expected) < 2000.0);
    }

    // Sweep repeats: the next buffer should again start near -deviation/2.
    std::vector<Sample> buf2(10);
    gen.generate(buf2.data(), buf2.size());
    double restartFreq = instFreqHz(buf2[0], buf2[1], sampleRate);
    CHECK(std::abs(restartFreq - startFreq) < 2000.0);
  }

  // A negative deviation must produce a down-chirp (frequency decreasing).
  {
    GeneratorConfig cfg;
    cfg.type = WaveformType::Chirp;
    cfg.sampleRateHz = sampleRate;
    cfg.chirpDeviationHz = -400e3;
    cfg.chirpDurationSec = 4096.0 / sampleRate;
    cfg.amplitude = 1.0f;
    SignalGenerator gen(cfg);

    std::vector<Sample> buf(4096);
    gen.generate(buf.data(), buf.size());

    double freqStart = instFreqHz(buf[5], buf[6], sampleRate);
    double freqEnd = instFreqHz(buf[3000], buf[3001], sampleRate);
    CHECK(freqStart > freqEnd);
    CHECK(std::abs(freqStart - 200e3) < 2000.0);
  }
}
