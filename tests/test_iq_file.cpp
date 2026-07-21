#include <cstdio>
#include <vector>

#include "../src/dsp/iq_file.h"
#include "test_framework.h"

using namespace iqforge;

namespace {
constexpr const char* kRoundtripPath = "iqforge_test_roundtrip.cf32";
}

void run_iq_file_tests() {
  // cf32 round trip: save then load back and compare exactly (float32 is lossless here).
  {
    SampleBuffer original = {{0.1f, -0.2f}, {1.0f, -1.0f}, {0.0f, 0.0f}, {-0.5f, 0.75f}};
    saveIqFileCf32(kRoundtripPath, original.data(), original.size());

    SampleBuffer loaded = loadIqFile(kRoundtripPath, IqFileFormat::Cf32);
    CHECK(loaded.size() == original.size());
    for (size_t i = 0; i < original.size(); ++i) {
      CHECK(loaded[i].real() == original[i].real());
      CHECK(loaded[i].imag() == original[i].imag());
    }
    std::remove(kRoundtripPath);
  }

  // Format guessing from extension.
  {
    CHECK(guessFormatFromExtension("foo.cf32") == IqFileFormat::Cf32);
    CHECK(guessFormatFromExtension("foo.fc32") == IqFileFormat::Cf32);
    CHECK(guessFormatFromExtension("foo.ci16") == IqFileFormat::Ci16);
    CHECK(guessFormatFromExtension("foo.sc16") == IqFileFormat::Ci16);
    CHECK(guessFormatFromExtension("foo.wav") == IqFileFormat::Wav);
    bool threw = false;
    try {
      guessFormatFromExtension("foo.unknown");
    } catch (const std::runtime_error&) {
      threw = true;
    }
    CHECK(threw);
  }

  // IQFileSource looping behavior.
  {
    SampleBuffer data = {{1, 0}, {2, 0}, {3, 0}};
    IQFileSource looping(data, /*loop=*/true);
    std::vector<Sample> out(7);
    size_t n = looping.generate(out.data(), out.size());
    CHECK(n == 7);
    // Should wrap: 1,2,3,1,2,3,1
    CHECK(out[0].real() == 1 && out[3].real() == 1 && out[6].real() == 1);

    IQFileSource oneShot(data, /*loop=*/false);
    std::vector<Sample> out2(7);
    size_t n2 = oneShot.generate(out2.data(), out2.size());
    CHECK(n2 == 3); // stops at EOF, doesn't wrap
  }
}
