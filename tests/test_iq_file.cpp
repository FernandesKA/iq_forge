#include <cstdio>
#include <vector>

#include "iq_file.h"
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

  // SigMF round trip: save with metadata + annotations, then reload via
  // both the .sigmf-data and .sigmf-meta paths and check everything survives.
  {
    constexpr const char* kBase = "iqforge_test_sigmf";
    SampleBuffer original = {{0.1f, -0.2f}, {1.0f, -1.0f}, {0.0f, 0.0f}, {-0.5f, 0.75f}};

    SigmfMeta meta;
    meta.sampleRateHz = 2.5e6;
    meta.hasSampleRate = true;
    meta.description = "test recording";
    meta.author = "tester";
    meta.recorder = "IQ Forge tests";
    meta.hw = "PlutoSDR (usb:1.5.5)";
    SigmfCapture cap;
    cap.sampleStart = 0;
    cap.frequencyHz = 915e6;
    cap.hasFrequency = true;
    cap.datetime = "2024-01-15T10:30:00.000Z";
    meta.captures.push_back(cap);
    SigmfAnnotation ann;
    ann.sampleStart = 1;
    ann.sampleCount = 2;
    ann.label = "burst";
    meta.annotations.push_back(ann);

    saveSigmf(kBase, original.data(), original.size(), meta);

    CHECK(isSigmfPath(std::string(kBase) + ".sigmf-data"));
    CHECK(isSigmfPath(std::string(kBase) + ".sigmf-meta"));
    CHECK(sigmfDataPath(std::string(kBase) + ".sigmf-meta") == std::string(kBase) + ".sigmf-data");
    CHECK(sigmfMetaPath(std::string(kBase) + ".sigmf-data") == std::string(kBase) + ".sigmf-meta");
    CHECK(guessFormatFromExtension(std::string(kBase) + ".sigmf-data") == IqFileFormat::Cf32);

    // Load via the .sigmf-data path...
    {
      auto [loaded, loadedMeta] = loadIqFileWithSigmf(std::string(kBase) + ".sigmf-data");
      CHECK(loaded.size() == original.size());
      for (size_t i = 0; i < original.size(); ++i) {
        CHECK(loaded[i].real() == original[i].real());
        CHECK(loaded[i].imag() == original[i].imag());
      }
      CHECK(loadedMeta.has_value());
      CHECK(loadedMeta->datatype == "cf32_le");
      CHECK(loadedMeta->hasSampleRate);
      CHECK(loadedMeta->sampleRateHz == 2.5e6);
      CHECK(loadedMeta->description == "test recording");
      CHECK(loadedMeta->hw == "PlutoSDR (usb:1.5.5)");
      CHECK(loadedMeta->captures.size() == 1);
      CHECK(loadedMeta->captures[0].hasFrequency);
      CHECK(loadedMeta->captures[0].frequencyHz == 915e6);
      CHECK(loadedMeta->captures[0].datetime == "2024-01-15T10:30:00.000Z");
      CHECK(loadedMeta->annotations.size() == 1);
      CHECK(loadedMeta->annotations[0].sampleStart == 1);
      CHECK(loadedMeta->annotations[0].sampleCount == 2);
      CHECK(loadedMeta->annotations[0].label == "burst");
    }

    // ...and via the sibling .sigmf-meta path -- should resolve to the same data.
    {
      auto [loaded, loadedMeta] = loadIqFileWithSigmf(std::string(kBase) + ".sigmf-meta");
      CHECK(loaded.size() == original.size());
      CHECK(loadedMeta.has_value());
    }

    // A plain (non-SigMF) path loaded through the same entry point should
    // just behave like loadIqFile(), with nullopt metadata.
    {
      constexpr const char* kPlainPath = "iqforge_test_sigmf_plain.cf32";
      saveIqFileCf32(kPlainPath, original.data(), original.size());
      auto [loaded, loadedMeta] = loadIqFileWithSigmf(kPlainPath);
      CHECK(loaded.size() == original.size());
      CHECK(!loadedMeta.has_value());
      std::remove(kPlainPath);
    }

    std::remove((std::string(kBase) + ".sigmf-data").c_str());
    std::remove((std::string(kBase) + ".sigmf-meta").c_str());
  }

  // isoTimestampNowUtc() shape: "YYYY-MM-DDTHH:MM:SS.SSSZ" (24 characters).
  {
    std::string ts = isoTimestampNowUtc();
    CHECK(ts.size() == 24);
    CHECK(ts[4] == '-' && ts[7] == '-' && ts[10] == 'T' && ts[13] == ':' && ts[16] == ':' && ts[19] == '.');
    CHECK(ts.back() == 'Z');
  }
}
