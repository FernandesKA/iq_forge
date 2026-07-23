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

pkg_check_modules(GLFW REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(LIBIIO REQUIRED IMPORTED_TARGET libiio)
pkg_check_modules(FFTW3F REQUIRED IMPORTED_TARGET fftw3f)

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

include(GNUInstallDirs)
include(FetchContent)

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

add_subdirectory(src)

install(TARGETS iqforge
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES README.md
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "iqforge")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SDR signal capture/transmit GUI")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/your-org/iqforge")
set(CPACK_PACKAGE_CONTACT "Maintainer <maintainer@localhost>")
set(CPACK_DEBIAN_PACKAGE_NAME "iqforge")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Maintainer <maintainer@localhost>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "SDR signal capture/transmit GUI")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

if(IQFORGE_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
