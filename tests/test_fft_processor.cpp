#include <algorithm>
#include <cmath>
#include <vector>

#include "../src/dsp/fft_processor.h"
#include "test_framework.h"

using namespace iqforge;

void run_fft_processor_tests() {
  constexpr size_t fftSize = 1024;

  // A constant (DC) input should produce a spectral peak exactly at the
  // shifted DC bin (fftSize/2).
  {
    FftProcessor fft({fftSize, WindowType::Rectangular, 1.0f});
    std::vector<Sample> in(fftSize, Sample(1.0f, 0.0f));
    std::vector<float> db;
    fft.process(in.data(), in.size(), db);
    size_t peak = static_cast<size_t>(std::max_element(db.begin(), db.end()) - db.begin());
    CHECK(peak == fftSize / 2);
  }

  // Fewer samples than fftSize must still work (zero-padded internally).
  {
    FftProcessor fft({fftSize, WindowType::Hann, 1.0f});
    std::vector<Sample> in(fftSize / 4, Sample(0.3f, 0.0f));
    std::vector<float> db;
    fft.process(in.data(), in.size(), db);
    CHECK(db.size() == fftSize);
  }

  // averagingAlpha < 1 should smooth a step change across two calls, i.e.
  // the second call's spectrum should sit strictly between the first two
  // "raw" states rather than jumping instantly.
  {
    FftProcessor fft({fftSize, WindowType::Rectangular, 0.2f});
    std::vector<Sample> silence(fftSize, Sample(0.0f, 0.0f));
    std::vector<Sample> loud(fftSize, Sample(1.0f, 0.0f));
    std::vector<float> db1, db2;
    fft.process(silence.data(), silence.size(), db1);
    fft.process(loud.data(), loud.size(), db2);
    // With alpha=0.2, the DC bin after the second call should be well below
    // what a fresh (unaveraged) transform of `loud` alone would show.
    FftProcessor freshFft({fftSize, WindowType::Rectangular, 1.0f});
    std::vector<float> dbFresh;
    freshFft.process(loud.data(), loud.size(), dbFresh);
    CHECK(db2[fftSize / 2] < dbFresh[fftSize / 2] - 1.0f);
  }
}
