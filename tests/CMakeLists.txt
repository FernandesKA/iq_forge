add_executable(iqforge_tests
  test_main.cpp
  test_ring_buffer.cpp
  test_signal_generator.cpp
  test_iq_file.cpp
  test_fft_processor.cpp
  test_resampler.cpp
  test_app_settings.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../common/gui/src/app_state.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../common/gui/src/app_settings.cpp
)
target_link_libraries(iqforge_tests PRIVATE iqforge_core)
# app_state.h/app_settings.h (GUI-framework-agnostic despite living under
# gui/) pull in duration_input.h/freq_input.h for their unit enums.
target_include_directories(iqforge_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common/gui/inc)

add_test(NAME iqforge_tests COMMAND iqforge_tests)
