add_library(iqforge_core STATIC
  dsp/signal_generator.cpp
  dsp/iq_file.cpp
  dsp/fft_processor.cpp
  devices/pluto_device.cpp
  devices/hackrf_device.cpp
  devices/device_manager.cpp
  devices/device_scanner.cpp
)
target_include_directories(iqforge_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iqforge_core PUBLIC
  PkgConfig::LIBIIO
  PkgConfig::LIBHACKRF
  PkgConfig::FFTW3F
  Threads::Threads
)

add_executable(iqforge
  main.cpp
  gui/app.cpp
  gui/app_state.cpp
  gui/log_panel.cpp
  gui/freq_input.cpp
  gui/duration_input.cpp
  gui/panel_device.cpp
  gui/panel_tx.cpp
  gui/panel_rx.cpp
  gui/plot_time_domain.cpp
  gui/plot_spectrum.cpp
  gui/plot_waterfall.cpp
)
target_link_libraries(iqforge PRIVATE iqforge_core implot imgui)
