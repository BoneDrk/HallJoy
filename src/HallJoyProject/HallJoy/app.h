#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int App_Run(HINSTANCE hInst, int nCmdShow);
void App_ForceFinalShutdown() noexcept;
void App_DisarmShutdownWatchdog() noexcept;
bool App_RequiresImmediateProcessExit() noexcept;
bool App_TakeRelaunchRequest() noexcept;
bool App_RelaunchSelf() noexcept;
constexpr UINT WM_APP_BLOCK_KEYS_CHANGED = WM_APP + 365;
// Shortcuts use the packed halljoy::shortcuts format (HID key + modifiers).
// ERROR_ALREADY_ASSIGNED: another command already uses the same shortcut.
DWORD App_SetBlockKeysHotkey(UINT shortcut);
DWORD App_ValidatePauseShortcut(unsigned slot, unsigned shortcut);
DWORD App_BlockKeysHotkeyError();
// One capture at a time. The owner receives halljoy::shortcuts::kCaptureMessage
// with lParam = packed shortcut or kCaptureCancelled (Esc / focus loss).
void App_BeginShortcutCapture(HWND owner);
void App_CancelShortcutCapture(bool notifyOwner);
void App_EndShortcutCapture(HWND owner);
