#include "file_browser.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace iqforge {

namespace {
namespace fs = std::filesystem;

struct Entry {
  std::string name;
  bool isDir;
};

struct BrowserState {
  fs::path currentDir;
  std::vector<Entry> entries;
  std::string selectedFile; // filename only within currentDir; empty = none
  std::string error;
};

void refresh(BrowserState& state) {
  state.entries.clear();
  state.error.clear();
  state.selectedFile.clear();

  std::error_code ec;
  fs::directory_iterator it(state.currentDir, fs::directory_options::skip_permission_denied, ec);
  if (ec) {
    state.error = ec.message();
    return;
  }
  for (const auto& entry : it) {
    std::error_code statEc;
    bool isDir = entry.is_directory(statEc);
    state.entries.push_back({entry.path().filename().string(), isDir});
  }
  std::sort(state.entries.begin(), state.entries.end(), [](const Entry& a, const Entry& b) {
    if (a.isDir != b.isDir) return a.isDir > b.isDir; // directories first
    return a.name < b.name;
  });
}
} // namespace

bool DrawIqFileBrowserPopup(const char* popupId, bool requestOpen, char* pathBuffer, size_t pathBufferSize) {
  static BrowserState state;

  if (requestOpen) {
    std::error_code ec;
    fs::path start = pathBuffer[0] ? fs::path(pathBuffer).parent_path() : fs::current_path();
    if (start.empty() || !fs::is_directory(start, ec)) start = fs::current_path();
    state.currentDir = fs::absolute(start, ec);
    refresh(state);
    ImGui::OpenPopup(popupId);
  }

  bool confirmed = false;
  ImGui::SetNextWindowSize(ImVec2(520, 420), ImGuiCond_FirstUseEver);
  if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::TextUnformatted(state.currentDir.string().c_str());
    ImGui::Separator();

    if (ImGui::Selectable("..", false, ImGuiSelectableFlags_DontClosePopups)) {
      fs::path parent = state.currentDir.parent_path();
      if (!parent.empty() && parent != state.currentDir) {
        state.currentDir = parent;
        refresh(state);
      }
    }

    ImGui::BeginChild("##fileList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2), true);
    for (const auto& entry : state.entries) {
      if (entry.isDir) {
        std::string label = "[dir] " + entry.name;
        if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_DontClosePopups)) {
          state.currentDir /= entry.name;
          refresh(state);
        }
      } else {
        bool selected = (state.selectedFile == entry.name);
        if (ImGui::Selectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
          state.selectedFile = entry.name;
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) confirmed = true;
        }
      }
    }
    ImGui::EndChild();

    if (!state.error.empty()) {
      ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), "%s", state.error.c_str());
    }

    ImGui::BeginDisabled(state.selectedFile.empty());
    if (ImGui::Button("Open")) confirmed = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();

    if (confirmed) {
      fs::path full = state.currentDir / state.selectedFile;
      std::snprintf(pathBuffer, pathBufferSize, "%s", full.string().c_str());
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  return confirmed;
}

} // namespace iqforge
