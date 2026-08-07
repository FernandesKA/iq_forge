#pragma once

#include <atomic>
#include <optional>
#include <string>
#include <thread>

#include "iq_file.h"
#include "resampler.h"
#include "sample_types.h"

namespace iqforge {

// Loads (and optionally resamples) an IQ file on a background thread, so a
// large file or a slow high-ratio resample -- resampleIq()'s sinc
// convolution over a whole file can easily take many seconds -- can't freeze
// the GUI thread the way calling loadIqFileWithSigmf()/resampleIq() directly
// from a button handler used to.
//
// Usage: call start() (e.g. from a Load button), then poll running()/
// finished() once per frame; once finished() is true, take() hands back the
// Result exactly once. Only one job is in flight per instance -- start()
// is a no-op (returns false) while a previous job is still running, so
// callers should disable their Load button while running() is true rather
// than queueing another start().
class AsyncIqLoadJob {
 public:
  struct Result {
    bool success = false;
    std::string errorMessage;
    SampleBuffer buffer;
    std::optional<SigmfMeta> sigmf;
    // Effective source rate actually used for the resample below: a SigMF
    // sidecar's core:sample_rate overrides whatever rate the caller passed
    // in, since it's authoritative where a bare .cf32/.ci16 has no rate of
    // its own to go on. Equal to buffer's rate whether or not resampling
    // was enabled.
    double sourceRateHz = 0.0;
    // == sourceRateHz if resampling was disabled, otherwise the rate
    // `buffer` was actually resampled to (sourceRateHz * the coefficient
    // passed to start()).
    double resultRateHz = 0.0;
    // Sample count before resampling (== buffer.size() if resampling was
    // disabled) -- kept around only so callers can log the "before -> after"
    // sample count the way the old synchronous Load handlers did.
    size_t inputSampleCount = 0;
  };

  ~AsyncIqLoadJob();

  // Starts loading `path` on a new thread. If `resampleEnabled`, the loaded
  // buffer is resampled from its effective source rate (see sourceRateHz
  // above) to (effective source rate * resampleCoefficient) afterwards.
  // `fallbackSourceRateHz` is used verbatim if the file has no SigMF sidecar
  // (or the sidecar has no sample rate) to recover one from.
  bool start(std::string path, bool resampleEnabled, double fallbackSourceRateHz, double resampleCoefficient,
             ResampleQuality quality = ResampleQuality::Best);

  bool running() const { return running_.load(std::memory_order_relaxed); }
  bool finished() const { return finished_.load(std::memory_order_acquire); }
  // Coarse status text for a progress bar's overlay -- "reading" and
  // "resampling" are the only two phases.
  const char* stage() const { return stage_.load(std::memory_order_relaxed); }
  // 0..1 fraction of the resample step done so far (see resampleIq()'s
  // onProgress); stays 0 during the "Reading file..." stage, since
  // loadIqFileWithSigmf() is a single unchunked read with nothing finer to
  // report -- in practice that stage is fast enough not to need a fraction
  // of its own. Meaningless once finished() is true.
  float progress() const { return progress_.load(std::memory_order_relaxed); }

  // Valid only once finished() is true. Joins the worker thread (already
  // done by the time finished() is observably true, so this doesn't block)
  // and moves the result out -- call exactly once per completed job.
  Result take();

 private:
  std::thread thread_;
  std::atomic<bool> running_{false};
  std::atomic<bool> finished_{false};
  std::atomic<const char*> stage_{""};
  std::atomic<float> progress_{0.0f};
  // Written only by the worker thread, and only before it stores
  // finished_ = true (release); read only by the GUI thread, and only after
  // it observes finished() == true (acquire) via take() -- so plain
  // (non-atomic) access here is safe despite crossing threads.
  Result result_;
};

} // namespace iqforge
