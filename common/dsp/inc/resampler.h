#pragma once

#include "sample_types.h"

namespace iqforge {

// Quality/speed tradeoff for resampleIq() below -- all three are windowed-
// sinc converters (libsamplerate's *_SINC_* modes), so all apply a correctly
// scaled anti-aliasing filter; they only differ in filter length (and so in
// CPU cost). "Best" is the right default for a one-shot file load.
enum class ResampleQuality { Best, Medium, Fastest };

// Resamples `input` from inputRateHz to outputRateHz using libsamplerate,
// treating I/Q as an interleaved 2-channel signal (exactly the layout
// std::complex<float> already has, and the same trick this codebase already
// relies on when handing Sample* to ImPlot -- see plot_time_domain.cpp).
// Throws std::runtime_error if the rates are invalid or the conversion
// ratio is outside libsamplerate's supported range (roughly 1:256..256:1).
SampleBuffer resampleIq(const SampleBuffer& input, double inputRateHz, double outputRateHz,
                         ResampleQuality quality = ResampleQuality::Best);

} // namespace iqforge
