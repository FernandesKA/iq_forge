#pragma once

#include <cstddef>

namespace iqforge {

// Folder-browsing popup for picking an IQ file, since typing a raw path by
// hand is error-prone. Call every frame regardless of whether it's open;
// pass requestOpen=true the frame a "Browse..." button is clicked to show
// it. When the user confirms a file, writes the chosen path into
// pathBuffer (including the null terminator, up to pathBufferSize) and
// returns true that same frame.
bool DrawIqFileBrowserPopup(const char* popupId, bool requestOpen, char* pathBuffer, size_t pathBufferSize);

} // namespace iqforge
