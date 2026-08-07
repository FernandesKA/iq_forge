#pragma once

namespace iqforge {

// True if `windowName`'s docked tab is the one currently selected/visible in
// its dock node -- i.e. the tab a user would actually see in front, not just
// present somewhere in a tab bar. False if the window hasn't been drawn yet
// this session, or isn't docked into a tabbed node at all.
bool isTabActive(const char* windowName);

} // namespace iqforge
