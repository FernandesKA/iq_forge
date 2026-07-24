#include "measurements.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>

#include "plot_format.h"

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

} // namespace

SpectrumMeasurements computeMeasurements(const std::vector<float>& db, double sampleRateHz,
                                          const std::vector<Sample>& timeDomain) {
  SpectrumMeasurements m;
  int n = static_cast<int>(db.size());
  if (n == 0) return m;
  m.valid = true;

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

  m.hasTimeStats = !timeDomain.empty();
  if (m.hasTimeStats) {
    double sumSq = 0.0;
    float peakMag = 0.0f;
    std::complex<double> sumC(0.0, 0.0);
    long clip = 0;
    for (const Sample& s : timeDomain) {
      float re = s.real(), im = s.imag();
      float mag2 = re * re + im * im;
      sumSq += mag2;
      peakMag = std::max(peakMag, std::sqrt(mag2));
      sumC += std::complex<double>(re, im);
      if (std::abs(re) >= kClipThreshold || std::abs(im) >= kClipThreshold) ++clip;
    }
    auto count = static_cast<double>(timeDomain.size());
    double rms = std::sqrt(sumSq / count);
    double dcMag = std::abs(sumC) / count;

    m.rmsDbFs = static_cast<float>(20.0 * std::log10(std::max(rms, 1e-15)));
    m.timePeakDbFs = static_cast<float>(20.0 * std::log10(std::max(static_cast<double>(peakMag), 1e-15)));
    m.crestFactorDb = m.timePeakDbFs - m.rmsDbFs;
    m.dcOffsetDbFs = static_cast<float>(20.0 * std::log10(std::max(dcMag, 1e-15)));
    m.clippingCount = clip;
    m.clippingPct = 100.0 * static_cast<double>(clip) / count;
  }

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

  row("Peak freq", formatHz(m.peakFreqHz).c_str());
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.peakLevelDbFs);
  row("Peak level", buf);
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.noiseFloorDbFs);
  row("Noise floor", buf);
  std::snprintf(buf, sizeof buf, "%.1f dB", m.snrDb);
  row("SNR", buf);
  row("Occupied BW", formatHz(m.occupiedBwHz).c_str());
  row("-3 dB BW", formatHz(m.bw3dBHz).c_str());
  row("-6 dB BW", formatHz(m.bw6dBHz).c_str());
  std::snprintf(buf, sizeof buf, "%.1f dBFS", m.channelPowerDbFs);
  row("Channel power", buf);

  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  ImGui::Separator();
  ImGui::TableSetColumnIndex(1);
  ImGui::Separator();

  if (m.hasTimeStats) {
    std::snprintf(buf, sizeof buf, "%.1f dBFS", m.rmsDbFs);
    row("RMS", buf);
    std::snprintf(buf, sizeof buf, "%.1f dBFS", m.timePeakDbFs);
    row("Peak (time)", buf);
    std::snprintf(buf, sizeof buf, "%.1f dB", m.crestFactorDb);
    row("Crest factor", buf);
    std::snprintf(buf, sizeof buf, "%.1f dBFS", m.dcOffsetDbFs);
    row("DC offset", buf);
    std::snprintf(buf, sizeof buf, "%ld (%.2f%%)", m.clippingCount, m.clippingPct);
    row("Clipping", buf);
  } else {
    row("RMS / peak / crest", "-");
    row("DC offset / clipping", "-");
  }

  ImGui::EndTable();
}

} // namespace iqforge
