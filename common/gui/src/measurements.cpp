#include "measurements.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>

namespace iqforge {

namespace {

double binToFreq(int bin, double sampleRateHz, int n) {
  if (n <= 0) return 0.0;
  return -sampleRateHz / 2.0 + static_cast<double>(bin) * (sampleRateHz / n);
}

// Samples this close to +-1.0 full scale reflect a saturated ADC/DAC code
// (e.g. int16 +-32767 round-trips to ~0.99997 through the normalization in
// hackrf_device.cpp/pluto_device.cpp), not just a signal that happens to
// swing wide.
constexpr float kClipThreshold = 0.999f;

struct FixedFreqUnit {
  double scale;
  const char* suffix;
};

// One unit for the whole table (picked from the Nyquist span, since every
// frequency-domain row is bounded by it), instead of formatHz() picking a
// unit per value -- otherwise "Peak freq" and "Occupied BW" could land in
// different units and be impossible to compare at a glance.
FixedFreqUnit pickFixedFreqUnit(double sampleRateHz) {
  double nyquist = std::abs(sampleRateHz) * 0.5;
  if (nyquist >= 1e9) return {1e9, "GHz"};
  if (nyquist >= 1e6) return {1e6, "MHz"};
  if (nyquist >= 1e3) return {1e3, "kHz"};
  return {1.0, "Hz"};
}

} // namespace

TimeDomainStats computeTimeDomainStats(const Sample* data, size_t count) {
  TimeDomainStats s;
  if (count == 0) return s;
  s.valid = true;

  double sumSq = 0.0;
  float peakMag = 0.0f;
  std::complex<double> sumC(0.0, 0.0);
  long clip = 0;
  for (size_t i = 0; i < count; ++i) {
    float re = data[i].real(), im = data[i].imag();
    float mag2 = re * re + im * im;
    sumSq += mag2;
    peakMag = std::max(peakMag, std::sqrt(mag2));
    sumC += std::complex<double>(re, im);
    if (std::abs(re) >= kClipThreshold || std::abs(im) >= kClipThreshold) ++clip;
  }
  auto n = static_cast<double>(count);
  double rms = std::sqrt(sumSq / n);
  double dcMag = std::abs(sumC) / n;

  s.rmsDbFs = static_cast<float>(20.0 * std::log10(std::max(rms, 1e-15)));
  s.peakDbFs = static_cast<float>(20.0 * std::log10(std::max(static_cast<double>(peakMag), 1e-15)));
  s.crestFactorDb = s.peakDbFs - s.rmsDbFs;
  s.dcOffsetDbFs = static_cast<float>(20.0 * std::log10(std::max(dcMag, 1e-15)));
  s.clippingCount = clip;
  s.clippingPct = 100.0 * static_cast<double>(clip) / n;
  return s;
}

SpectrumMeasurements computeMeasurements(const std::vector<float>& db, double sampleRateHz,
                                          const std::vector<Sample>& timeDomain) {
  SpectrumMeasurements m;
  int n = static_cast<int>(db.size());
  if (n == 0) return m;
  m.valid = true;
  m.sampleRateHz = sampleRateHz;

  int peakBin = static_cast<int>(std::max_element(db.begin(), db.end()) - db.begin());
  m.peakFreqHz = binToFreq(peakBin, sampleRateHz, n);
  m.peakLevelDbFs = db[peakBin];

  std::vector<float> sorted(db);
  std::sort(sorted.begin(), sorted.end());
  m.noiseFloorDbFs = sorted[sorted.size() / 2];
  m.snrDb = m.peakLevelDbFs - m.noiseFloorDbFs;

  double totalLinear = 0.0;
  std::vector<double> cum(n);
  for (int i = 0; i < n; ++i) {
    totalLinear += std::pow(10.0, db[i] / 10.0);
    cum[i] = totalLinear;
  }
  m.channelPowerDbFs = static_cast<float>(10.0 * std::log10(std::max(totalLinear, 1e-30)));

  // Occupied bandwidth: narrowest band whose cumulative power is 99% of the
  // total, trimming 0.5% off each tail.
  {
    double loTarget = totalLinear * 0.005;
    double hiTarget = totalLinear * 0.995;
    int loBin = 0, hiBin = n - 1;
    for (int i = 0; i < n; ++i) {
      if (cum[i] >= loTarget) {
        loBin = i;
        break;
      }
    }
    for (int i = n - 1; i >= 0; --i) {
      if (cum[i] <= hiTarget) {
        hiBin = i;
        break;
      }
    }
    if (hiBin < loBin) hiBin = loBin;
    m.occupiedBwHz = binToFreq(hiBin, sampleRateHz, n) - binToFreq(loBin, sampleRateHz, n);
  }

  // N dB bandwidth: walk outward from the peak until the trace falls more
  // than N dB below it on each side (main-lobe width, not a multi-lobe
  // aware measurement).
  auto bwAtThreshold = [&](float thresholdDb) {
    float level = m.peakLevelDbFs - thresholdDb;
    int lo = peakBin;
    while (lo > 0 && db[lo - 1] >= level) --lo;
    int hi = peakBin;
    while (hi < n - 1 && db[hi + 1] >= level) ++hi;
    return binToFreq(hi, sampleRateHz, n) - binToFreq(lo, sampleRateHz, n);
  };
  m.bw3dBHz = bwAtThreshold(3.0f);
  m.bw6dBHz = bwAtThreshold(6.0f);

  m.timeStats = computeTimeDomainStats(timeDomain.data(), timeDomain.size());

  return m;
}

void drawMeasurementsTable(const SpectrumMeasurements& m) {
  if (!m.valid) {
    ImGui::TextDisabled("No data");
    return;
  }
  if (!ImGui::BeginTable("##measurements", 2, ImGuiTableFlags_SizingFixedFit)) return;

  auto row = [](const char* label, const char* value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::TextUnformatted(value);
  };
  char buf[32];
  FixedFreqUnit unit = pickFixedFreqUnit(m.sampleRateHz);
  auto freqRow = [&](const char* label, double hz) {
    std::snprintf(buf, sizeof buf, "%.3f %s", hz / unit.scale, unit.suffix);
    row(label, buf);
  };

  freqRow("Peak freq", m.peakFreqHz);
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.peakLevelDbFs);
  row("Peak level", buf);
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.noiseFloorDbFs);
  row("Noise floor", buf);
  std::snprintf(buf, sizeof buf, "%.1f dB", m.snrDb);
  row("SNR", buf);
  freqRow("Occupied BW", m.occupiedBwHz);
  freqRow("-3 dB BW", m.bw3dBHz);
  freqRow("-6 dB BW", m.bw6dBHz);
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.channelPowerDbFs);
  row("Channel power", buf);

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Separator();
  ImGui::TableSetColumnIndex(1);
  ImGui::Separator();

  const TimeDomainStats& ts = m.timeStats;
  if (ts.valid) {
    std::snprintf(buf, sizeof buf, "%.1f dBFS", ts.rmsDbFs);
    row("RMS", buf);
    std::snprintf(buf, sizeof buf, "%.1f dBFS", ts.peakDbFs);
    row("Peak (time)", buf);
    std::snprintf(buf, sizeof buf, "%.1f dB", ts.crestFactorDb);
    row("Crest factor", buf);
    std::snprintf(buf, sizeof buf, "%.1f dBFS", ts.dcOffsetDbFs);
    row("DC offset", buf);
    std::snprintf(buf, sizeof buf, "%ld (%.2f%%)", ts.clippingCount, ts.clippingPct);
    row("Clipping", buf);
  } else {
    row("RMS / peak / crest", "-");
    row("DC offset / clipping", "-");
  }

  ImGui::EndTable();
}

} // namespace iqforge
