#include "app_settings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace iqforge {

namespace {
namespace fs = std::filesystem;

// %APPDATA%\iqforge on Windows, $XDG_CONFIG_HOME/iqforge (or ~/.config/iqforge)
// elsewhere -- same convention iq_file.h's SigMF paths don't need to care
// about since those are always explicit user-chosen paths, but settings/
// presets have no such path to anchor on. IQFORGE_CONFIG_DIR overrides this
// outright when set, so tests (and anyone who wants config elsewhere) don't
// have to touch the real per-user config location.
fs::path configDir() {
  if (const char* dirOverride = std::getenv("IQFORGE_CONFIG_DIR"); dirOverride && *dirOverride) {
    return fs::path(dirOverride);
  }
#if defined(_WIN32)
  const char* appData = std::getenv("APPDATA");
  fs::path base = appData ? fs::path(appData) : fs::path(".");
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  fs::path base;
  if (xdg && *xdg) {
    base = fs::path(xdg);
  } else {
    const char* home = std::getenv("HOME");
    base = home ? fs::path(home) / ".config" : fs::path(".");
  }
#endif
  return base / "iqforge";
}

fs::path prefsPath() { return configDir() / "prefs.json"; }
fs::path sessionPath() { return configDir() / "session.json"; }
fs::path presetsPath() { return configDir() / "presets.json"; }

bool readJsonFile(const fs::path& path, nlohmann::json& out) {
  std::ifstream f(path);
  if (!f) return false;
  try {
    f >> out;
  } catch (const std::exception&) {
    return false; // corrupt/partial file -- treat like "not found"
  }
  return true;
}

// Creates configDir() on demand (once per process is plenty; these are only
// ever called from user-triggered saves or the once-per-run auto-save) and
// reports whether the file was actually written.
bool writeJsonFile(const fs::path& path, const nlohmann::json& j) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  std::ofstream f(path, std::ios::trunc);
  if (!f) return false;
  f << j.dump(2);
  return f.good();
}

template <typename Buf>
void setBuf(Buf& buf, const std::string& s) {
  std::snprintf(buf, sizeof(buf), "%s", s.c_str());
}

const char* freqUnitName(FreqUnit u) {
  switch (u) {
    case FreqUnit::Hz: return "Hz";
    case FreqUnit::kHz: return "kHz";
    case FreqUnit::MHz: return "MHz";
    case FreqUnit::GHz: return "GHz";
  }
  return "MHz";
}
FreqUnit freqUnitFromName(const std::string& s, FreqUnit fallback) {
  if (s == "Hz") return FreqUnit::Hz;
  if (s == "kHz") return FreqUnit::kHz;
  if (s == "MHz") return FreqUnit::MHz;
  if (s == "GHz") return FreqUnit::GHz;
  return fallback;
}

const char* timeUnitName(TimeUnit u) {
  switch (u) {
    case TimeUnit::Ns: return "ns";
    case TimeUnit::Us: return "us";
    case TimeUnit::Ms: return "ms";
    case TimeUnit::S: return "s";
  }
  return "ms";
}
TimeUnit timeUnitFromName(const std::string& s, TimeUnit fallback) {
  if (s == "ns") return TimeUnit::Ns;
  if (s == "us") return TimeUnit::Us;
  if (s == "ms") return TimeUnit::Ms;
  if (s == "s") return TimeUnit::S;
  return fallback;
}

const char* deviceKindName(DeviceKind k) { return k == DeviceKind::HackRF ? "HackRF" : "PlutoSDR"; }
DeviceKind deviceKindFromName(const std::string& s, DeviceKind fallback) {
  if (s == "HackRF") return DeviceKind::HackRF;
  if (s == "PlutoSDR") return DeviceKind::PlutoSDR;
  return fallback;
}

const char* waveformName(WaveformType t) {
  switch (t) {
    case WaveformType::Tone: return "Tone";
    case WaveformType::MultiTone: return "MultiTone";
    case WaveformType::Chirp: return "Chirp";
    case WaveformType::Pulse: return "Pulse";
    case WaveformType::Barker: return "Barker";
    case WaveformType::Noise: return "Noise";
    case WaveformType::Ramp: return "Ramp";
  }
  return "Tone";
}
WaveformType waveformFromName(const std::string& s, WaveformType fallback) {
  if (s == "Tone") return WaveformType::Tone;
  if (s == "MultiTone") return WaveformType::MultiTone;
  if (s == "Chirp") return WaveformType::Chirp;
  if (s == "Pulse") return WaveformType::Pulse;
  if (s == "Barker") return WaveformType::Barker;
  if (s == "Noise") return WaveformType::Noise;
  if (s == "Ramp") return WaveformType::Ramp;
  return fallback;
}

const char* barkerCodeName(BarkerCode c) {
  switch (c) {
    case BarkerCode::B2PlusMinus: return "B2PlusMinus";
    case BarkerCode::B2PlusPlus: return "B2PlusPlus";
    case BarkerCode::B3: return "B3";
    case BarkerCode::B4PlusPlusMinusPlus: return "B4PlusPlusMinusPlus";
    case BarkerCode::B4PlusPlusPlusMinus: return "B4PlusPlusPlusMinus";
    case BarkerCode::B5: return "B5";
    case BarkerCode::B7: return "B7";
    case BarkerCode::B11: return "B11";
    case BarkerCode::B13: return "B13";
  }
  return "B13";
}
BarkerCode barkerCodeFromName(const std::string& s, BarkerCode fallback) {
  if (s == "B2PlusMinus") return BarkerCode::B2PlusMinus;
  if (s == "B2PlusPlus") return BarkerCode::B2PlusPlus;
  if (s == "B3") return BarkerCode::B3;
  if (s == "B4PlusPlusMinusPlus") return BarkerCode::B4PlusPlusMinusPlus;
  if (s == "B4PlusPlusPlusMinus") return BarkerCode::B4PlusPlusPlusMinus;
  if (s == "B5") return BarkerCode::B5;
  if (s == "B7") return BarkerCode::B7;
  if (s == "B11") return BarkerCode::B11;
  if (s == "B13") return BarkerCode::B13;
  return fallback;
}

const char* envelopeShapeName(EnvelopeShape s) {
  switch (s) {
    case EnvelopeShape::Rectangular: return "Rectangular";
    case EnvelopeShape::Sinc: return "Sinc";
    case EnvelopeShape::Gaussian: return "Gaussian";
    case EnvelopeShape::Hann: return "Hann";
  }
  return "Rectangular";
}
EnvelopeShape envelopeShapeFromName(const std::string& s, EnvelopeShape fallback) {
  if (s == "Rectangular") return EnvelopeShape::Rectangular;
  if (s == "Sinc") return EnvelopeShape::Sinc;
  if (s == "Gaussian") return EnvelopeShape::Gaussian;
  if (s == "Hann") return EnvelopeShape::Hann;
  return fallback;
}

const char* rxSaveFormatName(AppState::RxSaveFormat f) {
  return f == AppState::RxSaveFormat::Cf32Raw ? "Cf32Raw" : "Sigmf";
}
AppState::RxSaveFormat rxSaveFormatFromName(const std::string& s, AppState::RxSaveFormat fallback) {
  if (s == "Cf32Raw") return AppState::RxSaveFormat::Cf32Raw;
  if (s == "Sigmf") return AppState::RxSaveFormat::Sigmf;
  return fallback;
}

nlohmann::json settingsToJson(const AppState& state) {
  nlohmann::json j;

  nlohmann::json dev;
  dev["kind"] = deviceKindName(state.selectedKind);
  dev["uri"] = std::string(state.uriBuffer);
  dev["sampleRateHz"] = state.sampleRateHz;
  dev["sampleRateUnit"] = freqUnitName(state.sampleRateUnit);
  dev["centerFreqHz"] = state.centerFreqHz;
  dev["centerFreqUnit"] = freqUnitName(state.centerFreqUnit);
  dev["bandwidthHz"] = state.bandwidthHz;
  dev["bandwidthUnit"] = freqUnitName(state.bandwidthUnit);
  dev["txGainDb"] = state.txGainDb;
  dev["rxGainDb"] = state.rxGainDb;
  dev["fftSize"] = state.fftSize;
  j["device"] = std::move(dev);

  const GeneratorConfig& g = state.genConfig;
  nlohmann::json gen;
  gen["type"] = waveformName(g.type);
  gen["toneFreqHz"] = g.toneFreqHz;
  gen["toneFreqUnit"] = freqUnitName(state.toneFreqUnit);
  gen["multiToneFreqsHz"] = g.multiToneFreqsHz;
  gen["multiToneUnit"] = freqUnitName(state.multiToneUnit);
  gen["chirpDeviationHz"] = g.chirpDeviationHz;
  gen["chirpDeviationUnit"] = freqUnitName(state.chirpDeviationUnit);
  gen["chirpDurationSec"] = g.chirpDurationSec;
  gen["chirpDurationUnit"] = timeUnitName(state.chirpDurationUnit);
  gen["barkerCode"] = barkerCodeName(g.barkerCode);
  gen["barkerChipRateHz"] = g.barkerChipRateHz;
  gen["barkerChipRateUnit"] = freqUnitName(state.barkerChipRateUnit);
  gen["pulseDurationSec"] = g.pulseDurationSec;
  gen["pulseDurationUnit"] = timeUnitName(state.pulseDurationUnit);
  gen["pulsePeriodSec"] = g.pulsePeriodSec;
  gen["pulsePeriodUnit"] = timeUnitName(state.pulsePeriodUnit);
  gen["envelopeShape"] = envelopeShapeName(g.envelopeShape);
  gen["envelopeEnabled"] = g.envelopeEnabled;
  gen["amplitude"] = g.amplitude;

  nlohmann::json file;
  file["path"] = std::string(state.filePathBuffer);
  file["loop"] = state.fileLoop;
  file["sourceRateHz"] = state.fileSourceRateHz;
  file["sourceRateUnit"] = freqUnitName(state.fileSourceRateUnit);
  file["resampleEnabled"] = state.fileResampleEnabled;
  file["resampleCoefficient"] = state.fileResampleCoefficient;

  nlohmann::json tx;
  tx["sourceMode"] = state.txSourceMode;
  tx["generator"] = std::move(gen);
  tx["file"] = std::move(file);
  j["tx"] = std::move(tx);

  nlohmann::json rx;
  rx["saveFormat"] = rxSaveFormatName(state.rxSaveFormat);
  rx["recordPath"] = std::string(state.rxRecordPathBuffer);
  rx["recordDescription"] = std::string(state.rxRecordDescriptionBuffer);
  j["rx"] = std::move(rx);

  return j;
}

void applySettingsJson(AppState& state, const nlohmann::json& j) {
  if (j.contains("device")) {
    const auto& dev = j.at("device");
    state.selectedKind = deviceKindFromName(dev.value("kind", std::string()), state.selectedKind);
    setBuf(state.uriBuffer, dev.value("uri", std::string()));
    state.sampleRateHz = dev.value("sampleRateHz", state.sampleRateHz);
    state.sampleRateUnit = freqUnitFromName(dev.value("sampleRateUnit", std::string()), state.sampleRateUnit);
    state.centerFreqHz = dev.value("centerFreqHz", state.centerFreqHz);
    state.centerFreqUnit = freqUnitFromName(dev.value("centerFreqUnit", std::string()), state.centerFreqUnit);
    state.bandwidthHz = dev.value("bandwidthHz", state.bandwidthHz);
    state.bandwidthUnit = freqUnitFromName(dev.value("bandwidthUnit", std::string()), state.bandwidthUnit);
    state.txGainDb = dev.value("txGainDb", state.txGainDb);
    state.rxGainDb = dev.value("rxGainDb", state.rxGainDb);
    int fftSize = dev.value("fftSize", state.fftSize);
    state.setFftSize(fftSize);
  }

  if (j.contains("tx")) {
    const auto& tx = j.at("tx");
    state.txSourceMode = tx.value("sourceMode", state.txSourceMode);

    if (tx.contains("generator")) {
      const auto& gen = tx.at("generator");
      GeneratorConfig g = state.genConfig;
      g.type = waveformFromName(gen.value("type", std::string()), g.type);
      g.toneFreqHz = gen.value("toneFreqHz", g.toneFreqHz);
      state.toneFreqUnit = freqUnitFromName(gen.value("toneFreqUnit", std::string()), state.toneFreqUnit);
      g.multiToneFreqsHz = gen.value("multiToneFreqsHz", g.multiToneFreqsHz);
      state.multiToneUnit = freqUnitFromName(gen.value("multiToneUnit", std::string()), state.multiToneUnit);
      g.chirpDeviationHz = gen.value("chirpDeviationHz", g.chirpDeviationHz);
      state.chirpDeviationUnit =
          freqUnitFromName(gen.value("chirpDeviationUnit", std::string()), state.chirpDeviationUnit);
      g.chirpDurationSec = gen.value("chirpDurationSec", g.chirpDurationSec);
      state.chirpDurationUnit =
          timeUnitFromName(gen.value("chirpDurationUnit", std::string()), state.chirpDurationUnit);
      g.barkerCode = barkerCodeFromName(gen.value("barkerCode", std::string()), g.barkerCode);
      g.barkerChipRateHz = gen.value("barkerChipRateHz", g.barkerChipRateHz);
      state.barkerChipRateUnit =
          freqUnitFromName(gen.value("barkerChipRateUnit", std::string()), state.barkerChipRateUnit);
      g.pulseDurationSec = gen.value("pulseDurationSec", g.pulseDurationSec);
      state.pulseDurationUnit =
          timeUnitFromName(gen.value("pulseDurationUnit", std::string()), state.pulseDurationUnit);
      g.pulsePeriodSec = gen.value("pulsePeriodSec", g.pulsePeriodSec);
      state.pulsePeriodUnit = timeUnitFromName(gen.value("pulsePeriodUnit", std::string()), state.pulsePeriodUnit);
      g.envelopeShape = envelopeShapeFromName(gen.value("envelopeShape", std::string()), g.envelopeShape);
      g.envelopeEnabled = gen.value("envelopeEnabled", g.envelopeEnabled);
      g.amplitude = gen.value("amplitude", g.amplitude);
      g.sampleRateHz = state.sampleRateHz;
      state.genConfig = g;
      state.generator->setConfig(g);
    }

    if (tx.contains("file")) {
      const auto& file = tx.at("file");
      setBuf(state.filePathBuffer, file.value("path", std::string(state.filePathBuffer)));
      state.fileLoop = file.value("loop", state.fileLoop);
      state.fileSourceRateHz = file.value("sourceRateHz", state.fileSourceRateHz);
      state.fileSourceRateUnit = freqUnitFromName(file.value("sourceRateUnit", std::string()), state.fileSourceRateUnit);
      state.fileResampleEnabled = file.value("resampleEnabled", state.fileResampleEnabled);
      state.fileResampleCoefficient = file.value("resampleCoefficient", state.fileResampleCoefficient);
    }
  }

  if (j.contains("rx")) {
    const auto& rx = j.at("rx");
    state.rxSaveFormat = rxSaveFormatFromName(rx.value("saveFormat", std::string()), state.rxSaveFormat);
    setBuf(state.rxRecordPathBuffer, rx.value("recordPath", std::string(state.rxRecordPathBuffer)));
    setBuf(state.rxRecordDescriptionBuffer, rx.value("recordDescription", std::string(state.rxRecordDescriptionBuffer)));
  }
}

} // namespace

bool loadAutoSaveEnabled() {
  nlohmann::json j;
  if (!readJsonFile(prefsPath(), j)) return true; // no prefs file yet -- default on
  return j.value("autoSaveEnabled", true);
}

void saveAutoSaveEnabled(bool enabled) {
  nlohmann::json j;
  j["autoSaveEnabled"] = enabled;
  writeJsonFile(prefsPath(), j);
}

void saveSessionSettings(const AppState& state) {
  if (!loadAutoSaveEnabled()) return;
  writeJsonFile(sessionPath(), settingsToJson(state));
}

bool loadSessionSettings(AppState& state) {
  if (!loadAutoSaveEnabled()) return false;
  nlohmann::json j;
  if (!readJsonFile(sessionPath(), j)) return false;
  applySettingsJson(state, j);
  return true;
}

std::vector<std::string> listPresets() {
  nlohmann::json j;
  if (!readJsonFile(presetsPath(), j) || !j.is_object()) return {};
  std::vector<std::string> names;
  names.reserve(j.size());
  for (auto it = j.begin(); it != j.end(); ++it) names.push_back(it.key());
  std::sort(names.begin(), names.end());
  return names;
}

void savePreset(const AppState& state, const std::string& name) {
  nlohmann::json all;
  readJsonFile(presetsPath(), all); // missing/corrupt file -> start fresh below
  if (!all.is_object()) all = nlohmann::json::object();
  all[name] = settingsToJson(state);
  writeJsonFile(presetsPath(), all);
}

bool loadPreset(AppState& state, const std::string& name) {
  nlohmann::json all;
  if (!readJsonFile(presetsPath(), all) || !all.is_object() || !all.contains(name)) return false;
  applySettingsJson(state, all.at(name));
  return true;
}

void deletePreset(const std::string& name) {
  nlohmann::json all;
  if (!readJsonFile(presetsPath(), all) || !all.is_object()) return;
  all.erase(name);
  writeJsonFile(presetsPath(), all);
}

} // namespace iqforge
