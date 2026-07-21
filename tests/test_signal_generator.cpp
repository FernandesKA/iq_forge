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
