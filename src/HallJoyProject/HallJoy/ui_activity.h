#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// Child visibility alone stays true when its top-level window is minimized.
// Call on the UI thread; input workers use the published preview HWND instead.
inline bool HallJoyUiVisible(HWND window) noexcept {
    if (!window || !IsWindowVisible(window)) return false;
    const HWND root=GetAncestor(window,GA_ROOT);
    return root && IsWindowVisible(root) && !IsIconic(root);
}
