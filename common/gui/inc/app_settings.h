#pragma once

#include <string>
#include <vector>

#include "app_state.h"

namespace iqforge {

// Whether settings persist across restarts. Always itself persisted
// (prefs.json), independent of its own value, so turning it off sticks
// across restarts too -- otherwise there'd be no way to keep it disabled.
// Defaults to true before any prefs file exists.
bool loadAutoSaveEnabled();
void saveAutoSaveEnabled(bool enabled);

// Last-session settings (session.json): device/TX/RX configuration only --
// connection state, buffers, recordings, and the log are never persisted.
// saveSessionSettings() is a no-op unless loadAutoSaveEnabled() is true.
void saveSessionSettings(const AppState& state);
bool loadSessionSettings(AppState& state); // true if a session was found and applied

// Named presets (presets.json). Independent of the auto-save toggle above --
// explicitly saving/loading/deleting a preset always works regardless of it.
std::vector<std::string> listPresets();
void savePreset(const AppState& state, const std::string& name);
bool loadPreset(AppState& state, const std::string& name); // false if not found
void deletePreset(const std::string& name);

} // namespace iqforge
