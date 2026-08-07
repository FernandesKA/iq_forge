#pragma once

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "sample_source.h"

namespace iqforge {

enum class IqFileFormat {
  Cf32, // raw interleaved float32 I,Q,I,Q,... (GNU Radio "complex64" convention)
  Ci16, // raw interleaved int16 I,Q,I,Q,... scaled to +-1 by /32768
  Wav,  // canonical PCM16 stereo WAV, left=I right=Q, scaled to +-1
};

// Guesses format from file extension: .cf32/.fc32 -> Cf32, .ci16/.sc16 -> Ci16,
// .wav -> Wav, .sigmf-data -> Cf32 (IQ Forge always writes SigMF datasets as
// cf32_le; use loadIqFileWithSigmf() below to honor a different datatype
// declared by an arbitrary .sigmf-meta). Throws std::runtime_error if the
// extension is unrecognized.
IqFileFormat guessFormatFromExtension(const std::string& path);

// Loads an entire IQ file into memory. Throws std::runtime_error on I/O or
// format errors.
SampleBuffer loadIqFile(const std::string& path);
SampleBuffer loadIqFile(const std::string& path, IqFileFormat format);

// Saves samples as raw interleaved float32 (Cf32) — the simplest lossless
// format, used for RX recordings.
void saveIqFileCf32(const std::string& path, const Sample* data, size_t count);

// --- SigMF (https://sigmf.org) ---
//
// A SigMF Recording is a pair of files sharing a base name: a `.sigmf-data`
// file (the exact same raw interleaved-sample bytes as Cf32/Ci16 above, just
// under a different extension) plus a `.sigmf-meta` JSON sidecar carrying
// the sample rate, center frequency, recording device, and annotations that
// a bare .cf32/.ci16 file has no way to store. IQ Forge always writes
// `core:datatype: cf32_le`; on load, whatever datatype the sidecar declares
// (cf32_le or ci16_le) is honored.

struct SigmfCapture {
  uint64_t sampleStart = 0;
  double frequencyHz = 0.0;
  bool hasFrequency = false;
  std::string datetime; // ISO-8601 UTC, e.g. "2024-01-15T10:30:00.000Z"; empty if unknown
};

struct SigmfAnnotation {
  uint64_t sampleStart = 0;
  uint64_t sampleCount = 0;
  std::string label;
  bool hasFreqRange = false;
  double freqLowerHz = 0.0;
  double freqUpperHz = 0.0;
};

struct SigmfMeta {
  std::string datatype = "cf32_le"; // SigMF core:datatype string
  double sampleRateHz = 0.0;
  bool hasSampleRate = false;
  std::string description;
  std::string author;
  std::string recorder;
  std::string hw;
  std::vector<SigmfCapture> captures;
  std::vector<SigmfAnnotation> annotations;
};

// True if `path` has a `.sigmf-data` or `.sigmf-meta` extension.
bool isSigmfPath(const std::string& path);

// Swaps a `.sigmf-meta`/`.sigmf-data` path for its sibling with the other
// extension (the base filename, everything before the extension, is
// unchanged either way).
std::string sigmfDataPath(const std::string& path);
std::string sigmfMetaPath(const std::string& path);

// Strips a trailing extension (anything after the last '.' that comes after
// the last path separator) so the result can be handed to saveSigmf() as
// `basePath` -- e.g. "rec.sigmf-data" and "rec.cf32" both become "rec". A
// path with no extension is returned unchanged.
std::string sigmfBasePath(const std::string& path);

// Parses a `.sigmf-meta` JSON file. Throws std::runtime_error on I/O or
// malformed-JSON errors.
SigmfMeta loadSigmfMeta(const std::string& metaPath);

// Current UTC time formatted for SigMF's core:datetime field:
// "YYYY-MM-DDTHH:MM:SS.SSSZ" (RFC 3339 / ISO-8601, 'Z' offset, millisecond
// fraction).
std::string isoTimestampNowUtc();

// Loads `path` as an ordinary IQ file (see loadIqFile() above), except that
// if `path` (or its `.sigmf-meta`/`.sigmf-data` sibling) is a SigMF
// Recording, the returned metadata reflects the parsed sidecar and the
// sample data is decoded using the datatype it declares rather than being
// guessed from the extension. The second element is nullopt for a plain
// .cf32/.ci16/.wav file.
std::pair<SampleBuffer, std::optional<SigmfMeta>> loadIqFileWithSigmf(const std::string& path);

// Writes a SigMF Recording: `basePath + ".sigmf-data"` (raw interleaved
// float32, i.e. identical bytes to saveIqFileCf32()) and
// `basePath + ".sigmf-meta"` (JSON). `meta.datatype` is forced to "cf32_le"
// before writing regardless of what was passed in, since the data file this
// function produces is always Cf32; the rest of `meta` (sample rate,
// captures, annotations, description, etc.) is used as-is.
void saveSigmf(const std::string& basePath, const Sample* data, size_t count, SigmfMeta meta);

// Plays back an in-memory IQ buffer as a sample source, optionally looping.
class IQFileSource : public ISampleSource {
 public:
  explicit IQFileSource(SampleBuffer data, bool loop = true)
      : data_(std::move(data)), loop_(loop) {
    if (data_.empty()) throw std::runtime_error("IQ file source has no samples");
  }

  // Convenience: load a file and wrap it in one call.
  static IQFileSource fromFile(const std::string& path, bool loop = true) {
    return IQFileSource(loadIqFile(path), loop);
  }

  size_t generate(Sample* out, size_t count) override;

  size_t totalSamples() const { return data_.size(); }
  size_t position() const { return pos_; }
  void reset() { pos_ = 0; }
  const SampleBuffer& data() const { return data_; }

 private:
  SampleBuffer data_;
  size_t pos_ = 0;
  bool loop_;
};

} // namespace iqforge
