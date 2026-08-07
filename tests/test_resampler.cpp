#include <cmath>
#include <vector>

#include "resampler.h"
#include "test_framework.h"

using namespace iqforge;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

SampleBuffer makeTone(double freqHz, double sampleRateHz, size_t count) {
  SampleBuffer out(count);
  double phaseStep = kTwoPi * freqHz / sampleRateHz;
  for (size_t i = 0; i < count; ++i) {
    double phase = phaseStep * static_cast<double>(i);
    out[i] = Sample(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
  }
  return out;
}

// Estimates a tone's frequency from consecutive-sample phase advance,
// averaged over the buffer (same derivation plot_instfreq.cpp uses) --
// avoids pulling FftProcessor/FFTW into this test just to read one peak bin.
double estimateToneFreqHz(const SampleBuffer& data, double sampleRateHz) {
  double sumDPhase = 0.0;
  for (size_t i = 1; i < data.size(); ++i) {
    double p0 = std::atan2(data[i - 1].imag(), data[i - 1].real());
    double p1 = std::atan2(data[i].imag(), data[i].real());
    double d = p1 - p0;
    while (d > kPi) d -= kTwoPi;
    while (d < -kPi) d += kTwoPi;
    sumDPhase += d;
  }
  double avgDPhase = sumDPhase / static_cast<double>(data.size() - 1);
  return avgDPhase / kTwoPi * sampleRateHz;
}
} // namespace

void run_resampler_tests() {
  // Upsampling should preserve the signal's real-world tone frequency, not
  // its normalized (cycles/sample) frequency -- i.e. the tone should read
  // back at the same Hz value once re-measured against the new rate.
  {
    constexpr double kInRate = 1e6;
    constexpr double kOutRate = 2.5e6;
    constexpr double kToneHz = 100e3;
    SampleBuffer in = makeTone(kToneHz, kInRate, 4000);

    SampleBuffer out = resampleIq(in, kInRate, kOutRate);

    // Output length should track the resampling ratio.
    double expectedLen = static_cast<double>(in.size()) * (kOutRate / kInRate);
    CHECK(std::abs(static_cast<double>(out.size()) - expectedLen) < expectedLen * 0.01);

    double measuredHz = estimateToneFreqHz(out, kOutRate);
    CHECK(std::abs(measuredHz - kToneHz) < 1e3); // within 1 kHz of the original tone
  }

  // Downsampling: same tone-preservation check in the other direction.
  {
    constexpr double kInRate = 3e6;
    constexpr double kOutRate = 1e6;
    constexpr double kToneHz = 50e3;
    SampleBuffer in = makeTone(kToneHz, kInRate, 6000);

    SampleBuffer out = resampleIq(in, kInRate, kOutRate);

    double expectedLen = static_cast<double>(in.size()) * (kOutRate / kInRate);
    CHECK(std::abs(static_cast<double>(out.size()) - expectedLen) < expectedLen * 0.01);

    double measuredHz = estimateToneFreqHz(out, kOutRate);
    CHECK(std::abs(measuredHz - kToneHz) < 1e3);
  }

  // Equal rates: no-op fast path returns the input unchanged (same size,
  // same content), rather than running it through the sinc filter.
  {
    SampleBuffer in = makeTone(10e3, 1e6, 500);
    SampleBuffer out = resampleIq(in, 1e6, 1e6);
    CHECK(out.size() == in.size());
    CHECK(out[100] == in[100]);
  }

  // Empty input -> empty output, no throw.
  {
    SampleBuffer out = resampleIq({}, 1e6, 2e6);
    CHECK(out.empty());
  }

  // Invalid rates must throw rather than divide by zero / produce garbage.
  {
    bool threw = false;
    try {
      resampleIq(makeTone(1e3, 1e6, 10), 0.0, 1e6);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK(threw);
  }

  // A ratio outside libsamplerate's supported range must throw instead of
  // silently producing a degenerate (near-zero-length or huge) buffer.
  {
    bool threw = false;
    try {
      resampleIq(makeTone(1e3, 1e6, 10), 1e6, 1e9);
    } catch (const std::exception&) {
      threw = true;
    }
    CHECK(threw);
  }

  // resampleIq() processes the input in fixed-size chunks against
  // libsamplerate's streaming API (purely so onProgress can report partial
  // completion -- see resampler.h) rather than one single-shot call, so
  // correctness has to hold up across a chunk boundary too, not just for an
  // input that fits in one chunk. 200k frames spans multiple 64k-frame
  // chunks plus a partial one.
  {
    constexpr double kInRate = 2e6;
    constexpr double kOutRate = 1e6;
    constexpr double kToneHz = 75e3;
    SampleBuffer in = makeTone(kToneHz, kInRate, 200000);

    std::vector<float> progressCalls;
    SampleBuffer out = resampleIq(in, kInRate, kOutRate, ResampleQuality::Best,
                                   [&](float frac) { progressCalls.push_back(frac); });

    double expectedLen = static_cast<double>(in.size()) * (kOutRate / kInRate);
    CHECK(std::abs(static_cast<double>(out.size()) - expectedLen) < expectedLen * 0.01);
    double measuredHz = estimateToneFreqHz(out, kOutRate);
    CHECK(std::abs(measuredHz - kToneHz) < 1e3);

    // Multiple chunks -> multiple progress reports, ending at exactly done,
    // and never going backwards (each chunk only adds to input consumed).
    CHECK(progressCalls.size() > 1);
    CHECK(progressCalls.back() == 1.0f);
    for (size_t i = 1; i < progressCalls.size(); ++i) CHECK(progressCalls[i] >= progressCalls[i - 1]);
    for (float f : progressCalls) CHECK(f >= 0.0f && f <= 1.0f);
  }

  // No-op (ratio == 1) fast path still reports 100% so a caller relying on
  // onProgress to know when it's done isn't left hanging.
  {
    bool calledWithOne = false;
    resampleIq(makeTone(10e3, 1e6, 500), 1e6, 1e6, ResampleQuality::Best,
               [&](float frac) { calledWithOne = (frac == 1.0f); });
    CHECK(calledWithOne);
  }
}
