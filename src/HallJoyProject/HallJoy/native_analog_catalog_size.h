#pragma once
#include <cstddef>
// One compiled manifest drives storage, lifecycle and diagnostics. No manual cap.
inline constexpr std::size_t kNativeAnalogCatalogSize = 0
#define HALLJOY_NATIVE_BACKEND(getter) + 1
#include "native_analog_backends.def"
#undef HALLJOY_NATIVE_BACKEND
;
static_assert(kNativeAnalogCatalogSize > 0);
