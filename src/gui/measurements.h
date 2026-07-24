#pragma once

#include <vector>

#include "../core/sample_types.h"

namespace iqforge {

// A snapshot of standard spectrum/time-domain measurements, computed fresh
// each frame from the current spectrum trace and raw IQ buffer -- unlike
// markers, these need no placement and always describe "the signal that's
// there right now".
struct SpectrumMeasurements {
  bool valid = false;      // false if there's no spectrum data yet
  bool hasTimeStats = false; // false if there's no time-domain data yet

  // Kept only so drawMeasurementsTable() can pick one fixed frequency unit
  // for the whole table (Nyquist-scaled) instead of each row auto-scaling
  // its own unit independently, which made rows hard to compare at a glance.
  double sampleRateHz = 0.0;

  // Frequency-domain (from the dB trace).
  double peakFreqHz = 0.0;
  float peakLevelDbFs = 0.0f;
  float noiseFloorDbFs = 0.0f; // median of the trace -- robust to a handful of narrowband peaks
  float snrDb = 0.0f;          // peakLevelDbFs - noiseFloorDbFs
  double occupiedBwHz = 0.0;   // narrowest band containing 99% of total power
  double bw3dBHz = 0.0;        // width around the peak where the trace stays within 3 dB of it
  double bw6dBHz = 0.0;        // ... within 6 dB
  float channelPowerDbFs = 0.0f; // total power integrated across the full displayed span

  // Time-domain (from the raw IQ buffer).
  float rmsDbFs = 0.0f;
  float timePeakDbFs = 0.0f;
  float crestFactorDb = 0.0f; // timePeakDbFs - rmsDbFs
  float dcOffsetDbFs = 0.0f;  // |mean(samples)|, relative to full scale
  long clippingCount = 0;     // samples with |I| or |Q| >= 0.999 (near full scale)
  double clippingPct = 0.0;
};

// `db` is the same fftshifted baseband dB trace plotSpectrum() draws;
// `timeDomain` is the raw (untriggered) rolling IQ buffer for the same
// direction (TX or RX), used for the RMS/peak/crest/DC/clipping stats that
// can't be read off the spectrum.
SpectrumMeasurements computeMeasurements(const std::vector<float>& db, double sampleRateHz,
                                          const std::vector<Sample>& timeDomain);

// Draws the compact two-column measurements table. Call inside whatever
// width-constrained region (e.g. a child window) it should occupy.
void drawMeasurementsTable(const SpectrumMeasurements& m);

} // namespace iqforge
