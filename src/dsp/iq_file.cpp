#include "iq_file.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace iqforge {

namespace {

std::string lowerExt(const std::string& path) {
  auto dot = path.find_last_of('.');
  if (dot == std::string::npos) return "";
  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return std::tolower(c); });
  return ext;
}

std::vector<uint8_t> readWholeFile(const std::string& path) {
  std::ifstream f(path, std::ios::binary | std::ios::ate);
  if (!f) throw std::runtime_error("Cannot open IQ file: " + path);
  std::streamsize size = f.tellg();
  if (size < 0) throw std::runtime_error("Cannot determine size of IQ file: " + path);
  f.seekg(0, std::ios::beg);
  std::vector<uint8_t> buf(static_cast<size_t>(size));
  if (size > 0 && !f.read(reinterpret_cast<char*>(buf.data()), size)) {
    throw std::runtime_error("Failed reading IQ file: " + path);
  }
  return buf;
}

SampleBuffer parseCf32(const std::vector<uint8_t>& bytes) {
  if (bytes.size() % (2 * sizeof(float)) != 0) {
    throw std::runtime_error("cf32 file size is not a multiple of 8 bytes (I/Q float32 pairs)");
  }
  size_t n = bytes.size() / (2 * sizeof(float));
  SampleBuffer out(n);
  const float* f = reinterpret_cast<const float*>(bytes.data());
  for (size_t i = 0; i < n; ++i) out[i] = Sample(f[2 * i], f[2 * i + 1]);
  return out;
}

SampleBuffer parseCi16(const std::vector<uint8_t>& bytes) {
  if (bytes.size() % (2 * sizeof(int16_t)) != 0) {
    throw std::runtime_error("ci16 file size is not a multiple of 4 bytes (I/Q int16 pairs)");
  }
  size_t n = bytes.size() / (2 * sizeof(int16_t));
  SampleBuffer out(n);
  const int16_t* s = reinterpret_cast<const int16_t*>(bytes.data());
  constexpr float kScale = 1.0f / 32768.0f;
  for (size_t i = 0; i < n; ++i) {
    out[i] = Sample(s[2 * i] * kScale, s[2 * i + 1] * kScale);
  }
  return out;
}

uint32_t readU32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint16_t readU16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

SampleBuffer parseWav(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < 12 || std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
      std::memcmp(bytes.data() + 8, "WAVE", 4) != 0) {
    throw std::runtime_error("Not a valid RIFF/WAVE file");
  }

  uint16_t numChannels = 0;
  uint16_t bitsPerSample = 0;
  uint16_t audioFormat = 0;
  const uint8_t* dataPtr = nullptr;
  size_t dataSize = 0;

  size_t pos = 12;
  while (pos + 8 <= bytes.size()) {
    char id[5] = {0};
    std::memcpy(id, bytes.data() + pos, 4);
    uint32_t chunkSize = readU32(bytes.data() + pos + 4);
    size_t bodyStart = pos + 8;
    if (bodyStart + chunkSize > bytes.size()) break;

    if (std::memcmp(id, "fmt ", 4) == 0 && chunkSize >= 16) {
      const uint8_t* f = bytes.data() + bodyStart;
      audioFormat = readU16(f + 0);
      numChannels = readU16(f + 2);
      bitsPerSample = readU16(f + 14);
    } else if (std::memcmp(id, "data", 4) == 0) {
      dataPtr = bytes.data() + bodyStart;
      dataSize = chunkSize;
    }

    pos = bodyStart + chunkSize + (chunkSize % 2); // chunks are word-aligned
  }

  if (dataPtr == nullptr) throw std::runtime_error("WAV file has no data chunk");
  if (audioFormat != 1 && audioFormat != 0xFFFE) {
    throw std::runtime_error("Only PCM WAV files are supported");
  }
  if (bitsPerSample != 16) {
    throw std::runtime_error("Only 16-bit WAV files are supported");
  }
  if (numChannels != 1 && numChannels != 2) {
    throw std::runtime_error("Only mono or stereo WAV files are supported");
  }

  constexpr float kScale = 1.0f / 32768.0f;
  const int16_t* samples = reinterpret_cast<const int16_t*>(dataPtr);
  size_t frameCount = dataSize / (numChannels * sizeof(int16_t));
  SampleBuffer out(frameCount);
  for (size_t i = 0; i < frameCount; ++i) {
    if (numChannels == 2) {
      out[i] = Sample(samples[2 * i] * kScale, samples[2 * i + 1] * kScale);
    } else {
      out[i] = Sample(samples[i] * kScale, 0.0f);
    }
  }
  return out;
}

} // namespace

IqFileFormat guessFormatFromExtension(const std::string& path) {
  std::string ext = lowerExt(path);
  if (ext == "cf32" || ext == "fc32") return IqFileFormat::Cf32;
  if (ext == "ci16" || ext == "sc16") return IqFileFormat::Ci16;
  if (ext == "wav") return IqFileFormat::Wav;
  throw std::runtime_error("Cannot guess IQ file format from extension: ." + ext);
}

SampleBuffer loadIqFile(const std::string& path) {
  return loadIqFile(path, guessFormatFromExtension(path));
}

SampleBuffer loadIqFile(const std::string& path, IqFileFormat format) {
  std::vector<uint8_t> bytes = readWholeFile(path);
  switch (format) {
    case IqFileFormat::Cf32: return parseCf32(bytes);
    case IqFileFormat::Ci16: return parseCi16(bytes);
    case IqFileFormat::Wav: return parseWav(bytes);
  }
  throw std::runtime_error("Unknown IQ file format");
}

void saveIqFileCf32(const std::string& path, const Sample* data, size_t count) {
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) throw std::runtime_error("Cannot create IQ file: " + path);
  for (size_t i = 0; i < count; ++i) {
    float iq[2] = {data[i].real(), data[i].imag()};
    f.write(reinterpret_cast<const char*>(iq), sizeof(iq));
  }
  if (!f) throw std::runtime_error("Failed writing IQ file: " + path);
}

size_t IQFileSource::generate(Sample* out, size_t count) {
  size_t written = 0;
  while (written < count) {
    if (pos_ >= data_.size()) {
      if (!loop_) break;
      pos_ = 0;
    }
    size_t chunk = std::min(count - written, data_.size() - pos_);
    std::memcpy(out + written, data_.data() + pos_, chunk * sizeof(Sample));
    pos_ += chunk;
    written += chunk;
  }
  return written;
}

} // namespace iqforge
