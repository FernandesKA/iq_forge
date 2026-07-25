#include "iq_file.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
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

bool endsWithCI(const std::string& s, const std::string& suffix) {
  if (s.size() < suffix.size()) return false;
  return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin(), [](unsigned char a, unsigned char b) {
    return std::tolower(a) == std::tolower(b);
  });
}

IqFileFormat sigmfDatatypeToFormat(const std::string& datatype) {
  if (datatype == "cf32_le") return IqFileFormat::Cf32;
  if (datatype == "ci16_le") return IqFileFormat::Ci16;
  throw std::runtime_error("Unsupported SigMF core:datatype: " + datatype);
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
  // IQ Forge always writes SigMF datasets as cf32_le; loadIqFileWithSigmf()
  // below honors whatever datatype an arbitrary .sigmf-meta actually declares.
  if (ext == "sigmf-data") return IqFileFormat::Cf32;
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

bool isSigmfPath(const std::string& path) {
  return endsWithCI(path, ".sigmf-data") || endsWithCI(path, ".sigmf-meta");
}

std::string sigmfDataPath(const std::string& path) {
  if (endsWithCI(path, ".sigmf-meta")) {
    return path.substr(0, path.size() - std::string(".sigmf-meta").size()) + ".sigmf-data";
  }
  return path;
}

std::string sigmfMetaPath(const std::string& path) {
  if (endsWithCI(path, ".sigmf-data")) {
    return path.substr(0, path.size() - std::string(".sigmf-data").size()) + ".sigmf-meta";
  }
  return path;
}

std::string isoTimestampNowUtc() {
  using namespace std::chrono;
  auto now = system_clock::now();
  auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  std::time_t t = system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  // 32 would fit the real (4-digit-year) output, but GCC's -Wformat-truncation
  // sizes against %d's theoretical worst case (an 11-digit signed int) for
  // every field and warns; oversize the buffer to clear that instead.
  char buf[80];
  std::snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms.count()));
  return buf;
}

SigmfMeta loadSigmfMeta(const std::string& metaPath) {
  std::ifstream f(metaPath);
  if (!f) throw std::runtime_error("Cannot open SigMF meta file: " + metaPath);
  nlohmann::json j;
  try {
    f >> j;
  } catch (const std::exception& e) {
    throw std::runtime_error("Malformed SigMF meta JSON (" + metaPath + "): " + e.what());
  }

  SigmfMeta meta;
  nlohmann::json global = j.value("global", nlohmann::json::object());
  meta.datatype = global.value("core:datatype", std::string("cf32_le"));
  if (global.contains("core:sample_rate")) {
    meta.sampleRateHz = global.at("core:sample_rate").get<double>();
    meta.hasSampleRate = true;
  }
  meta.description = global.value("core:description", std::string());
  meta.author = global.value("core:author", std::string());
  meta.recorder = global.value("core:recorder", std::string());
  meta.hw = global.value("core:hw", std::string());

  if (j.contains("captures") && j.at("captures").is_array()) {
    for (const auto& c : j.at("captures")) {
      SigmfCapture cap;
      cap.sampleStart = c.value("core:sample_start", uint64_t{0});
      if (c.contains("core:frequency")) {
        cap.frequencyHz = c.at("core:frequency").get<double>();
        cap.hasFrequency = true;
      }
      cap.datetime = c.value("core:datetime", std::string());
      meta.captures.push_back(std::move(cap));
    }
  }

  if (j.contains("annotations") && j.at("annotations").is_array()) {
    for (const auto& a : j.at("annotations")) {
      SigmfAnnotation ann;
      ann.sampleStart = a.value("core:sample_start", uint64_t{0});
      ann.sampleCount = a.value("core:sample_count", uint64_t{0});
      ann.label = a.value("core:label", std::string());
      if (a.contains("core:freq_lower_edge") && a.contains("core:freq_upper_edge")) {
        ann.freqLowerHz = a.at("core:freq_lower_edge").get<double>();
        ann.freqUpperHz = a.at("core:freq_upper_edge").get<double>();
        ann.hasFreqRange = true;
      }
      meta.annotations.push_back(std::move(ann));
    }
  }
  return meta;
}

std::pair<SampleBuffer, std::optional<SigmfMeta>> loadIqFileWithSigmf(const std::string& path) {
  if (!isSigmfPath(path)) {
    return {loadIqFile(path), std::nullopt};
  }
  SigmfMeta meta = loadSigmfMeta(sigmfMetaPath(path));
  SampleBuffer samples = loadIqFile(sigmfDataPath(path), sigmfDatatypeToFormat(meta.datatype));
  return {std::move(samples), std::move(meta)};
}

void saveSigmf(const std::string& basePath, const Sample* data, size_t count, SigmfMeta meta) {
  meta.datatype = "cf32_le";
  saveIqFileCf32(basePath + ".sigmf-data", data, count);

  nlohmann::json global;
  global["core:datatype"] = meta.datatype;
  global["core:version"] = "1.2.0";
  if (meta.hasSampleRate) global["core:sample_rate"] = meta.sampleRateHz;
  if (!meta.description.empty()) global["core:description"] = meta.description;
  if (!meta.author.empty()) global["core:author"] = meta.author;
  if (!meta.recorder.empty()) global["core:recorder"] = meta.recorder;
  if (!meta.hw.empty()) global["core:hw"] = meta.hw;

  nlohmann::json captures = nlohmann::json::array();
  for (const auto& c : meta.captures) {
    nlohmann::json cap;
    cap["core:sample_start"] = c.sampleStart;
    if (c.hasFrequency) cap["core:frequency"] = c.frequencyHz;
    if (!c.datetime.empty()) cap["core:datetime"] = c.datetime;
    captures.push_back(std::move(cap));
  }

  nlohmann::json annotations = nlohmann::json::array();
  for (const auto& a : meta.annotations) {
    nlohmann::json ann;
    ann["core:sample_start"] = a.sampleStart;
    ann["core:sample_count"] = a.sampleCount;
    if (!a.label.empty()) ann["core:label"] = a.label;
    if (a.hasFreqRange) {
      ann["core:freq_lower_edge"] = a.freqLowerHz;
      ann["core:freq_upper_edge"] = a.freqUpperHz;
    }
    annotations.push_back(std::move(ann));
  }

  nlohmann::json j;
  j["global"] = std::move(global);
  j["captures"] = std::move(captures);
  j["annotations"] = std::move(annotations);

  std::string metaPath = basePath + ".sigmf-meta";
  std::ofstream out(metaPath, std::ios::trunc);
  if (!out) throw std::runtime_error("Cannot create SigMF meta file: " + metaPath);
  out << j.dump(2);
  if (!out) throw std::runtime_error("Failed writing SigMF meta file: " + metaPath);
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
