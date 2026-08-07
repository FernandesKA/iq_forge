#include "async_iq_load.h"

#include <utility>

namespace iqforge {

AsyncIqLoadJob::~AsyncIqLoadJob() {
  if (thread_.joinable()) thread_.join();
}

bool AsyncIqLoadJob::start(std::string path, bool resampleEnabled, double fallbackSourceRateHz,
                            double resampleCoefficient, ResampleQuality quality) {
  if (running_.load(std::memory_order_relaxed)) return false;
  if (thread_.joinable()) thread_.join(); // previous job's thread has already finished; reclaim its handle

  running_.store(true, std::memory_order_relaxed);
  finished_.store(false, std::memory_order_relaxed);
  stage_.store("Reading file...", std::memory_order_relaxed);
  progress_.store(0.0f, std::memory_order_relaxed);
  result_ = Result{};

  thread_ = std::thread([this, path = std::move(path), resampleEnabled, fallbackSourceRateHz, resampleCoefficient,
                          quality] {
    try {
      auto [raw, sigmf] = loadIqFileWithSigmf(path);

      double sourceRateHz = fallbackSourceRateHz;
      if (sigmf && sigmf->hasSampleRate) sourceRateHz = sigmf->sampleRateHz;

      result_.inputSampleCount = raw.size();
      if (resampleEnabled) {
        stage_.store("Resampling...", std::memory_order_relaxed);
        double targetRateHz = sourceRateHz * resampleCoefficient;
        result_.buffer = resampleIq(raw, sourceRateHz, targetRateHz, quality, [this](float frac) {
          progress_.store(frac, std::memory_order_relaxed);
        });
        result_.resultRateHz = targetRateHz;
      } else {
        result_.buffer = std::move(raw);
        result_.resultRateHz = sourceRateHz;
      }
      result_.sourceRateHz = sourceRateHz;
      result_.sigmf = std::move(sigmf);
      result_.success = true;
    } catch (const std::exception& e) {
      result_.success = false;
      result_.errorMessage = e.what();
    }
    running_.store(false, std::memory_order_relaxed);
    finished_.store(true, std::memory_order_release);
  });
  return true;
}

AsyncIqLoadJob::Result AsyncIqLoadJob::take() {
  if (thread_.joinable()) thread_.join();
  finished_.store(false, std::memory_order_relaxed);
  return std::move(result_);
}

} // namespace iqforge
