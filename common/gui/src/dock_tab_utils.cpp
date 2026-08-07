#include "dock_tab_utils.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace iqforge {

bool isTabActive(const char* windowName) {
  ImGuiWindow* window = ImGui::FindWindowByName(windowName);
  if (!window || !window->DockNode) return false;
  return window->DockNode->SelectedTabId == window->TabId;
}

} // namespace iqforge
