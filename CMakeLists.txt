cmake_minimum_required(VERSION 3.16)
project(iqforge VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(IQFORGE_BUILD_TESTS "Build unit tests" ON)

find_package(PkgConfig REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

include(GNUInstallDirs)
include(FetchContent)

pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

if(WIN32)
  # Needed to compile the .rc resource that embeds assets/iqforge.ico into
  # iqforge.exe (see common/CMakeLists.txt) -- not implied by the C/CXX
  # languages above, and without it the built .exe (and any shortcut to it)
  # falls back to Windows' generic executable icon.
  enable_language(RC)

  # Neither libiio nor libhackrf have a vcpkg port, so they can't be pulled in
  # via the vcpkg/pkg-config path used on Linux. libiio ships prebuilt MSVC
  # binaries with its releases; libhackrf is a single hackrf.c/hackrf.h pair
  # that only depends on libusb (available via vcpkg), so it's built from
  # source instead.
  set(LIBIIO_VERSION "0.26")
  set(LIBIIO_PREBUILT_DIR "${CMAKE_BINARY_DIR}/_deps/libiio-prebuilt")
  if(NOT EXISTS "${LIBIIO_PREBUILT_DIR}/Windows-VS-2022-x64/libiio.dll")
    file(DOWNLOAD
      "https://github.com/analogdevicesinc/libiio/releases/download/v${LIBIIO_VERSION}/Windows.zip"
      "${CMAKE_BINARY_DIR}/_deps/libiio-windows.zip"
      SHOW_PROGRESS)
    file(ARCHIVE_EXTRACT
      INPUT "${CMAKE_BINARY_DIR}/_deps/libiio-windows.zip"
      DESTINATION "${LIBIIO_PREBUILT_DIR}")
  endif()
  if(NOT EXISTS "${LIBIIO_PREBUILT_DIR}/include/iio.h")
    file(DOWNLOAD
      "https://raw.githubusercontent.com/analogdevicesinc/libiio/v${LIBIIO_VERSION}/iio.h"
      "${LIBIIO_PREBUILT_DIR}/include/iio.h")
  endif()

  add_library(PkgConfig::LIBIIO SHARED IMPORTED)
  set_target_properties(PkgConfig::LIBIIO PROPERTIES
    IMPORTED_LOCATION "${LIBIIO_PREBUILT_DIR}/Windows-VS-2022-x64/libiio.dll"
    IMPORTED_IMPLIB "${LIBIIO_PREBUILT_DIR}/Windows-VS-2022-x64/libiio.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBIIO_PREBUILT_DIR}/include")

  FetchContent_Declare(
    hackrf
    GIT_REPOSITORY https://github.com/greatscottgadgets/hackrf.git
    GIT_TAG v2026.01.3
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR host/libhackrf
  )
  FetchContent_MakeAvailable(hackrf)

  # The project includes <libhackrf/hackrf.h>, but libhackrf's own build
  # exposes hackrf.h without that directory prefix. Mirror it into a
  # "libhackrf/" subdirectory so both platforms use the same include path.
  set(LIBHACKRF_SHIM_DIR "${CMAKE_BINARY_DIR}/_deps/libhackrf-shim")
  file(COPY "${hackrf_SOURCE_DIR}/host/libhackrf/src/hackrf.h"
       DESTINATION "${LIBHACKRF_SHIM_DIR}/libhackrf")
  target_include_directories(hackrf INTERFACE "$<BUILD_INTERFACE:${LIBHACKRF_SHIM_DIR}>")

  add_library(PkgConfig::LIBHACKRF ALIAS hackrf)
else()
  pkg_check_modules(LIBIIO REQUIRED IMPORTED_TARGET libiio)

  # libhackrf's .pc file advertises a -I.../libusb-1.0 flag that isn't actually
  # needed by libhackrf/hackrf.h (it doesn't include any libusb headers), but
  # requiring libusb-1.0-dev just to satisfy pkg-config's path-validation would
  # be an unnecessary system dependency. Find it directly instead.
  find_path(LIBHACKRF_INCLUDE_DIR NAMES libhackrf/hackrf.h)
  find_library(LIBHACKRF_LIBRARY NAMES hackrf)
  if(NOT LIBHACKRF_INCLUDE_DIR OR NOT LIBHACKRF_LIBRARY)
    message(FATAL_ERROR "libhackrf not found")
  endif()
  add_library(PkgConfig::LIBHACKRF INTERFACE IMPORTED)
  target_include_directories(PkgConfig::LIBHACKRF INTERFACE ${LIBHACKRF_INCLUDE_DIR})
  target_link_libraries(PkgConfig::LIBHACKRF INTERFACE ${LIBHACKRF_LIBRARY})
endif()

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.90.9-docking
)
FetchContent_Declare(
  implot
  GIT_REPOSITORY https://github.com/epezent/implot.git
  GIT_TAG v0.16
)
FetchContent_MakeAvailable(imgui implot)

add_library(imgui STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui PUBLIC PkgConfig::GLFW OpenGL::GL)

add_library(implot STATIC
  ${implot_SOURCE_DIR}/implot.cpp
  ${implot_SOURCE_DIR}/implot_items.cpp
  ${implot_SOURCE_DIR}/implot_demo.cpp
)
target_include_directories(implot PUBLIC ${implot_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)

# Used for IQ file resampling (see src/dsp/resampler.h).
set(LIBSAMPLERATE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBSAMPLERATE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  libsamplerate
  GIT_REPOSITORY https://github.com/libsndfile/libsamplerate.git
  GIT_TAG 0.2.2
  GIT_SHALLOW TRUE
)
# libsamplerate's own CMakeLists.txt declares cmake_minimum_required(VERSION
# 3.1...), and CMake >= 4.0 hard-errors on that instead of just warning.
# Widen what we accept for this one dependency rather than patching its file.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_MakeAvailable(libsamplerate)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# Used to read/write SigMF (.sigmf-meta) sidecar metadata (see src/dsp/iq_file.h).
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.12.0
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

add_subdirectory(common)

install(TARGETS iqforge
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

if(NOT WIN32)
  # Desktop-menu integration for the .deb package: without these, `apt
  # install`ing it drops a bare binary in /usr/bin with no launcher entry or
  # icon anywhere a desktop environment would find it.
  install(FILES ${PROJECT_SOURCE_DIR}/assets/iqforge.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
  )
  install(FILES ${PROJECT_SOURCE_DIR}/assets/iqforge.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
  )
endif()

if(WIN32)
  # The CI workflow copies all runtime DLLs (vcpkg deps, libiio, hackrf) next
  # to iqforge.exe in the build tree after building; install/CPack picks up
  # whatever is there at packaging time so it doesn't need to know their
  # individual source locations.
  install(DIRECTORY ${CMAKE_BINARY_DIR}/common/
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    FILES_MATCHING PATTERN "*.dll"
  )
endif()

# CI passes the actual release tag here (e.g. -DIQFORGE_PACKAGE_VERSION=1.2.3
# derived from a vX.Y.Z git tag, see .github/workflows/build.yml) so packages
# built for a release carry that version. Without this, CPack falls back to
# PROJECT_VERSION from the project() call at the top of this file, which is
# a fixed placeholder that never changes between tags -- every .deb/.exe
# would report "0.1.0" regardless of what was actually tagged.
set(IQFORGE_PACKAGE_VERSION "" CACHE STRING "Override CPACK_PACKAGE_VERSION (e.g. from a CI release tag)")
if(IQFORGE_PACKAGE_VERSION)
  set(CPACK_PACKAGE_VERSION "${IQFORGE_PACKAGE_VERSION}")
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "iqforge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR signal capture/transmit GUI")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/FernandesKA/iq_forge")
set(CPACK_PACKAGE_CONTACT "FernandesKA <fernandezka.2601@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_NAME "iqforge")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "FernandesKA <fernandezka.2601@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "SDR signal capture/transmit GUI")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_NSIS_PACKAGE_NAME "iqforge")
set(CPACK_NSIS_DISPLAY_NAME "iqforge")
set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/assets/iqforge.ico")
set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/assets/iqforge.ico")
set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\iqforge.exe")
set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
set(CPACK_NSIS_MODIFY_PATH ON)
set(CPACK_PACKAGE_EXECUTABLES "iqforge" "iqforge")
set(CPACK_CREATE_DESKTOP_LINKS "iqforge")

include(CPack)

if(IQFORGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
