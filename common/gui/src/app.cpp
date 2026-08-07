#include "app.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_internal.h>
#include <implot.h>

#include <cstdio>

#include "app_settings.h"
#include "log_panel.h"
#include "panel_device.h"
#include "panel_rx.h"
#include "panel_settings.h"
#include "panel_signal_viewer.h"
#include "panel_spectrum_viewer.h"
#include "panel_tx.h"
#include "panel_visualization.h"
#include "plot_zoom_controls.h"

namespace iqforge {

namespace {
void glfwErrorCallback(int error, const char* description) {
  std::fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

// Builds the default docked layout on first run: Device fixed at the top of
// the left column, TX/RX/Signal Viewer/Settings control panels tabbed
// together below it (only the active mode's controls take up space, instead
// of every mode's panel being permanently wedged into its own sliver — see
// syncModeTabs() below for what keeps the two tab bars in step as the user
// switches modes in either one), plots tabbed together in the main area, log
// along the bottom. Only runs once — after that the user's own arrangement
// (persisted in imgui.ini) takes over.
void buildDefaultLayout(ImGuiID dockspaceId) {
  ImGui::DockBuilderRemoveNode(dockspaceId);
  ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

  ImGuiID mainId = dockspaceId;
  ImGuiID left = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Left, 0.28f, nullptr, &mainId);
  ImGuiID bottom = ImGui::DockBuilderSplitNode(mainId, ImGuiDir_Down, 0.22f, nullptr, &mainId);

  ImGuiID leftTop = ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.3f, nullptr, &left);

  ImGui::DockBuilderDockWindow("Device", leftTop);
  ImGui::DockBuilderDockWindow("TX Control", left);
  ImGui::DockBuilderDockWindow("RX Control", left);
  ImGui::DockBuilderDockWindow("Signal Viewer Control", left);
  ImGui::DockBuilderDockWindow("Settings", left);
  ImGui::DockBuilderDockWindow("TX", mainId);
  ImGui::DockBuilderDockWindow("RX", mainId);
  ImGui::DockBuilderDockWindow("SpectrumViewer", mainId);
  ImGui::DockBuilderDockWindow("Signal Viewer", mainId);
  ImGui::DockBuilderDockWindow("Log", bottom);

  ImGui::DockBuilderFinish(dockspaceId);
}

// Ties each mode's main-area window to the control panel that configures it,
// so opening *either* one (clicking its tab, on the left or in the main
// area) brings the other to the front too -- otherwise the two tab bars are
// really the same "mode" choice presented twice, and the user has to click
// both separately to get a consistent view instead of one click doing it.
struct ModeLink {
  const char* mainWindow;
  const char* controlWindow;
};
constexpr ModeLink kModeLinks[] = {
    {"TX", "TX Control"},
    {"RX", "RX Control"},
    {"Signal Viewer", "Signal Viewer Control"},
};

ImGuiWindow* findDockedWindow(const char* name) {
  ImGuiWindow* window = ImGui::FindWindowByName(name);
  return (window && window->DockNode && window->DockNode->TabBar) ? window : nullptr;
}

bool isSelectedTab(ImGuiWindow* window) { return window && window->DockNode->SelectedTabId == window->TabId; }

void selectTab(ImGuiWindow* window) {
  if (window) window->DockNode->TabBar->NextSelectedTabId = window->TabId;
}

void syncModeTabs() {
  // Pointer identity is enough here -- kModeLinks' strings are literals with
  // static storage, so the same address recurs every frame a given mode
  // stays selected, and only actually changes when the active tab does.
  static const char* lastMain = nullptr;
  static const char* lastControl = nullptr;

  const char* activeMain = nullptr;
  const char* activeControl = nullptr;
  for (const ModeLink& link : kModeLinks) {
    if (isSelectedTab(findDockedWindow(link.mainWindow))) activeMain = link.mainWindow;
    if (isSelectedTab(findDockedWindow(link.controlWindow))) activeControl = link.controlWindow;
  }

  // Only one side can actually change from a real click in a given frame
  // (a user can't click two different tab bars at once), so whichever
  // side moved is the one to propagate -- checking main first is an
  // arbitrary tie-break for the one frame both might already disagree with
  // their last-seen value (e.g. right after startup), not a live race.
  if (activeMain && activeMain != lastMain) {
    for (const ModeLink& link : kModeLinks) {
      if (link.mainWindow == activeMain) {
        selectTab(findDockedWindow(link.controlWindow));
        activeControl = link.controlWindow; // reflect the change requested above so it isn't also (mis)detected as a control-side click below
        break;
      }
    }
  } else if (activeControl && activeControl != lastControl) {
    for (const ModeLink& link : kModeLinks) {
      if (link.controlWindow == activeControl) {
        selectTab(findDockedWindow(link.mainWindow));
        activeMain = link.mainWindow;
        break;
      }
    }
  }

  lastMain = activeMain;
  lastControl = activeControl;
}
} // namespace

App::App() = default;

App::~App() {
  saveSessionSettings(state_);
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
  configurePlotBoxSelect();

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init(glslVersion);

  loadSessionSettings(state_); // no-op if auto-save is disabled or no prior session exists

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
    drawSignalViewerPanel(state_);
    drawSettingsPanel(state_);
    drawTxVisualizationPanel(state_);
    drawRxVisualizationPanel(state_);
    drawSignalViewerVisualizationPanel(state_);
    drawSpectrumViewerPanel(state_);
    drawLogPanel(state_);

    syncModeTabs();

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
