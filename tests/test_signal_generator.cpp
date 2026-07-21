#include <algorithm>
#include <vector>

#include "../src/dsp/fft_processor.h"
#include "../src/dsp/signal_generator.h"
#include "test_framework.h"

using namespace iqforge;

namespace {
size_t peakBin(const std::vector<float>& db) {
  return static_cast<size_t>(std::max_element(db.begin(), db.end()) - db.begin());
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
    cfg.chirpStartFreqHz = -100e6;
    cfg.chirpEndFreqHz = 100e6;
    cfg.barkerChipRateHz = 100e6;

    SignalGenerator gen(cfg);
    GeneratorConfig actual = gen.config();
    CHECK(actual.toneFreqHz == sampleRate / 2.0);
    CHECK(actual.multiToneFreqsHz[0] == -sampleRate / 2.0);
    CHECK(actual.multiToneFreqsHz[1] == sampleRate / 2.0);
    CHECK(actual.chirpStartFreqHz == -sampleRate / 2.0);
    CHECK(actual.chirpEndFreqHz == sampleRate / 2.0);
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
}
