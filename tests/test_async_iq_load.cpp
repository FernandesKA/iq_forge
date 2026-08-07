#include <chrono>
#include <cstdio>
#include <thread>

#include "async_iq_load.h"
#include "test_framework.h"

using namespace iqforge;

namespace {
constexpr const char* kPath = "iqforge_test_async_load.cf32";

// Real background-thread completion has no fixed deadline, but a few KB at
// SRC_SINC_FASTEST on a background thread should never take anywhere near
// this long -- treat it as "job is stuck" rather than legitimately still
// working if hit.
void waitUntilFinished(AsyncIqLoadJob& job) {
  for (int i = 0; i < 2000 && !job.finished(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  CHECK(job.finished());
}
} // namespace

void run_async_iq_load_tests() {
  SampleBuffer original(2000);
  for (size_t i = 0; i < original.size(); ++i) {
    original[i] = Sample(static_cast<float>(i) / 2000.0f, -static_cast<float>(i) / 2000.0f);
  }
  saveIqFileCf32(kPath, original.data(), original.size());

  // No resample: the job should just hand back exactly what's on disk, same
  // as loadIqFile() would synchronously.
  {
    AsyncIqLoadJob job;
    CHECK(job.start(kPath, /*resampleEnabled=*/false, /*fallbackSourceRateHz=*/1e6, /*resampleCoefficient=*/1.0));
    waitUntilFinished(job);

    AsyncIqLoadJob::Result result = job.take();
    CHECK(result.success);
    CHECK(result.errorMessage.empty());
    CHECK(!result.sigmf.has_value()); // plain .cf32, no SigMF sidecar
    CHECK(result.sourceRateHz == 1e6);
    CHECK(result.resultRateHz == 1e6);
    CHECK(result.inputSampleCount == original.size());
    CHECK(result.buffer.size() == original.size());
    for (size_t i = 0; i < original.size(); ++i) CHECK(result.buffer[i] == original[i]);
  }

  // With resampling: output rate/length should reflect the coefficient, the
  // same math resampleIq() itself uses.
  {
    AsyncIqLoadJob job;
    CHECK(job.start(kPath, /*resampleEnabled=*/true, /*fallbackSourceRateHz=*/1e6, /*resampleCoefficient=*/2.0));
    waitUntilFinished(job);

    CHECK(job.progress() == 1.0f); // resampleIq()'s onProgress reaches 1.0 before the job reports finished

    AsyncIqLoadJob::Result result = job.take();
    CHECK(result.success);
    CHECK(result.sourceRateHz == 1e6);
    CHECK(result.resultRateHz == 2e6);
    CHECK(result.inputSampleCount == original.size());
    double expectedLen = static_cast<double>(original.size()) * 2.0;
    CHECK(std::abs(static_cast<double>(result.buffer.size()) - expectedLen) < expectedLen * 0.01);
  }

  // Nonexistent file: reported as a failed job, not a thrown/crashed thread.
  {
    AsyncIqLoadJob job;
    CHECK(job.start("iqforge_test_async_load_missing.cf32", false, 1e6, 1.0));
    waitUntilFinished(job);

    AsyncIqLoadJob::Result result = job.take();
    CHECK(!result.success);
    CHECK(!result.errorMessage.empty());
  }

  // A job can be reused for a second load after take() -- start() must not
  // get stuck thinking a previous (already-joined) thread is still around.
  {
    AsyncIqLoadJob job;
    CHECK(job.start(kPath, false, 1e6, 1.0));
    waitUntilFinished(job);
    job.take();

    CHECK(job.start(kPath, false, 1e6, 1.0));
    waitUntilFinished(job);
    AsyncIqLoadJob::Result result = job.take();
    CHECK(result.success);
    CHECK(result.buffer.size() == original.size());
  }

  std::remove(kPath);
}
