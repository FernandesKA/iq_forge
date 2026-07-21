#pragma once

#include <stdexcept>
#include <string>

#include "../core/sample_source.h"

namespace iqforge {

enum class IqFileFormat {
  Cf32, // raw interleaved float32 I,Q,I,Q,... (GNU Radio "complex64" convention)
  Ci16, // raw interleaved int16 I,Q,I,Q,... scaled to +-1 by /32768
  Wav,  // canonical PCM16 stereo WAV, left=I right=Q, scaled to +-1
};

// Guesses format from file extension: .cf32/.fc32 -> Cf32, .ci16/.sc16 -> Ci16,
// .wav -> Wav. Throws std::runtime_error if the extension is unrecognized.
IqFileFormat guessFormatFromExtension(const std::string& path);

// Loads an entire IQ file into memory. Throws std::runtime_error on I/O or
// format errors.
SampleBuffer loadIqFile(const std::string& path);
SampleBuffer loadIqFile(const std::string& path, IqFileFormat format);

// Saves samples as raw interleaved float32 (Cf32) — the simplest lossless
// format, used for RX recordings.
void saveIqFileCf32(const std::string& path, const Sample* data, size_t count);

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

 private:
  SampleBuffer data_;
  size_t pos_ = 0;
  bool loop_;
};

} // namespace iqforge
