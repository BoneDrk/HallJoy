#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

// Directory overrides isolate writer tests; production uses the app data root
// and mirrors continuous logs beside the executable.
// Structural events only. Never pass key codes, input values, paths or serials.
bool SupportLog_Start(const wchar_t* directoryOverride = nullptr, const wchar_t* mirrorOverride = nullptr) noexcept;
bool SupportLog_Stop() noexcept;
enum class SupportLogDetailKind : unsigned char { None, Data, Win32, Protocol };
struct SupportLogDetail { std::uint64_t value=0; SupportLogDetailKind kind=SupportLogDetailKind::None; };
constexpr SupportLogDetail SupportLog_Data(std::uint64_t v) { return {v,SupportLogDetailKind::Data}; }
constexpr SupportLogDetail SupportLog_Win32(std::uint64_t v) { return {v,SupportLogDetailKind::Win32}; }
constexpr SupportLogDetail SupportLog_Protocol(std::uint64_t v) { return {v,SupportLogDetailKind::Protocol}; }
// An untyped third integer is deliberately a compile error. Metadata is not an error.
void SupportLog_Event(const char* category, std::uint64_t value, SupportLogDetail detail = {}) noexcept;
void SupportLog_OverlaySummary(const wchar_t* aggregate) noexcept;
void SupportLog_InventoryChanged() noexcept;
void SupportLog_ReportMissingSource() noexcept;
void SupportLog_ReportFailure(const char* category, std::uint64_t error) noexcept;
void SupportLog_SetWindow(HWND window) noexcept;
DWORD SupportLog_LastError() noexcept;
std::uint64_t SupportLog_RequestSnapshot() noexcept;
std::uint64_t SupportLog_CompletedSnapshot() noexcept;
std::wstring SupportLog_Directory();
