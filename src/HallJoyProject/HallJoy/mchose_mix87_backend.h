#pragma once
#include "native_analog_backend.h"
enum class Mix87ModeState { Absent, Checking, Disabled, Enabled, Busy, Failed };
struct Mix87ModeSnapshot { Mix87ModeState state; std::uint64_t session; unsigned profile; };
Mix87ModeSnapshot MchoseMix87_GetMode();
bool MchoseMix87_RequestMode(Mix87ModeSnapshot expected, bool enabled);
const NativeAnalogBackendDescriptor& MchoseMix87_GetNativeBackendDescriptor();
