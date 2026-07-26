#include <algorithm>
#include <cmath>
#include <vector>

#include "fft_processor.h"
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

  // setFftSize() must rebuild the plan/buffers for the new size (not just
  // reinterpret the old ones): output length tracks the new size, and the
  // DC-bin-at-center property from the first test above must still hold.
  {
    FftProcessor fft({fftSize, WindowType::Rectangular, 1.0f});
    std::vector<Sample> in(fftSize, Sample(1.0f, 0.0f));
    std::vector<float> db;
    fft.process(in.data(), in.size(), db);
    CHECK(db.size() == fftSize);

    constexpr size_t newSize = 2048;
    fft.setFftSize(newSize);
    CHECK(fft.fftSize() == newSize);

    std::vector<Sample> in2(newSize, Sample(1.0f, 0.0f));
    fft.process(in2.data(), in2.size(), db);
    CHECK(db.size() == newSize);
    size_t peak = static_cast<size_t>(std::max_element(db.begin(), db.end()) - db.begin());
    CHECK(peak == newSize / 2);
  }

  // A no-op resize (same size) must not disturb an in-progress average.
  {
    FftProcessor fft({fftSize, WindowType::Rectangular, 0.2f});
    std::vector<Sample> silence(fftSize, Sample(0.0f, 0.0f));
    std::vector<Sample> loud(fftSize, Sample(1.0f, 0.0f));
    std::vector<float> db;
    fft.process(silence.data(), silence.size(), db);
    fft.setFftSize(fftSize); // no-op: same size
    fft.process(loud.data(), loud.size(), db);
    // Same assertion as the averaging test above: if the no-op resize had
    // reset the running average, this would come out equal to a fresh,
    // unaveraged transform of `loud` instead of still blending with the
    // earlier `silence` call.
    FftProcessor freshFft({fftSize, WindowType::Rectangular, 1.0f});
    std::vector<float> dbFresh;
    freshFft.process(loud.data(), loud.size(), dbFresh);
    CHECK(db[fftSize / 2] < dbFresh[fftSize / 2] - 1.0f);
  }
}
