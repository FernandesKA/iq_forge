#include "app.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <implot.h>

#include <cstdio>

#include "log_panel.h"
#include "panel_device.h"
#include "panel_rx.h"
#include "panel_tx.h"
#include "plot_spectrum.h"
#include "plot_time_domain.h"
#include "plot_waterfall.h"
#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
void glfwErrorCallback(int error, const char* description) {
  std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Builds the default docked layout on first run: device/TX/RX controls down
// the left column, plots tabbed together in the main area, log along the
// bottom. Only runs once — after that the user's own arrangement (persisted
// in imgui.ini) takes over.
void buildDefaultLayout(ImGuiID dockspaceId) {
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

  ImGuiID mainId = dockspaceId;
  ImGuiID left = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.28f, nullptr, &mainId);
  ImGuiID bottom = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.22f, nullptr, &mainId);

  ImGuiID leftTop = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.34f, nullptr, &left);
  ImGuiID leftMid = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.5f, nullptr, &left);

  ImGui::DockBuilderDockWindow("Device", leftTop);
  ImGui::DockBuilderDockWindow("TX", leftMid);
  ImGui::DockBuilderDockWindow("RX", left);
  ImGui::DockBuilderDockWindow("Time Domain", mainId);
  ImGui::DockBuilderDockWindow("Spectrum", mainId);
  ImGui::DockBuilderDockWindow("RX Waterfall", mainId);
  ImGui::DockBuilderDockWindow("Log", bottom);

  ImGui::DockBuilderFinish(dockspaceId);
}
} // namespace

App::App() = default;

App::~App() {
  state_.deviceManager.disconnect();

  if (window_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
  }
}

bool App::init() {
  glfwSetErrorCallback(glfwErrorCallback);
  if (!glfwInit()) return false;

  const char* glslVersion = "#version 130";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

  window_ = glfwCreateWindow(1280, 800, "IQ Forge", nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    return false;
  }
  glfwMakeContextCurrent(window_);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  configurePlotWheelZoom();

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  return true;
}

void App::run() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiID dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
    static bool layoutBuilt = false;
    if (!layoutBuilt) {
      layoutBuilt = true;
      buildDefaultLayout(dockspaceId);
    }

    state_.updateDisplays();

    drawDevicePanel(state_);
    drawTxPanel(state_);
    drawRxPanel(state_);
    drawTimeDomainPanel(state_);
    drawSpectrumPanel(state_);
    drawWaterfallPanel(state_);
    drawLogPanel(state_);

    ImGui::Render();
    int displayW, displayH;
    glfwGetFramebufferSize(window_, &displayW, &displayH);
    glViewport(0, 0, displayW, displayH);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window_);
  }
}

} // namespace iqforge
