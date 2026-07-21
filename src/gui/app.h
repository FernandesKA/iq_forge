#pragma once

#include "app_state.h"

struct GLFWwindow;

namespace iqforge {

// Owns the GLFW/OpenGL3/ImGui/ImPlot lifecycle and runs the main loop. All
// application state lives in AppState (see app_state.h); App just wires the
// window and drives per-frame updates + panel drawing.
class App {
 public:
  App();
  ~App();

  // Returns false if the window/GL context could not be created.
  bool init();

  // Runs until the window is closed.
  void run();

 private:
  GLFWwindow* window_ = nullptr;
  AppState state_;
};

} // namespace iqforge
