add_executable(iqforge_tests
  test_main.cpp
  test_ring_buffer.cpp
  test_signal_generator.cpp
  test_iq_file.cpp
  test_fft_processor.cpp
)
target_link_libraries(iqforge_tests PRIVATE iqforge_core)
target_include_directories(iqforge_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

add_test(NAME iqforge_tests COMMAND iqforge_tests)
