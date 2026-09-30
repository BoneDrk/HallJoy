#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>

// Engine state mirror and the one-way UI request used by Pause/Resume
// shortcuts. Key handling lives in input_shortcuts(_runtime).h, shared with
// the Block Bound Keys toggle.
namespace halljoy::pause_hotkey {
enum class State { Transition, Active, Paused };
enum class Action { None, Pause, Resume };
inline constexpr UINT Message = WM_APP + 381;
inline std::atomic<HWND> window{};
inline std::atomic<State> state{State::Transition};
inline std::atomic<std::uint64_t> generation{0};
}
