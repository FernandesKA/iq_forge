#include <cstdio>

#include "test_framework.h"

void run_ring_buffer_tests();
void run_signal_generator_tests();
void run_iq_file_tests();
void run_fft_processor_tests();
void run_resampler_tests();
void run_app_settings_tests();

int main() {
  run_ring_buffer_tests();
  run_signal_generator_tests();
  run_iq_file_tests();
  run_fft_processor_tests();
  run_resampler_tests();
  run_app_settings_tests();

  if (g_failures == 0) {
    std::printf("All tests passed.\n");
    return 0;
  }
  std::printf("%d check(s) failed.\n", g_failures);
  return 1;
}
