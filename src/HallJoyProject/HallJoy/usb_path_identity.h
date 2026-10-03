#pragma once
// USB identity in a Windows HID interface path ("...hid#vid_28e9&pid_3265&mi_02#...").
// Used only as a cheap pre-filter before opening a device: a path that
// carries a different USB VID/PID cannot be the device. A path without a
// parseable USB identity (e.g. Bluetooth "_vid&02xxxx") is not decided here
// and the caller must open and check it as before.
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace halljoy::usb_path {

constexpr wchar_t FoldAscii(wchar_t value) noexcept
{
    return value >= L'A' && value <= L'Z' ? static_cast<wchar_t>(value - L'A' + L'a') : value;
}

constexpr bool TokenAt(std::wstring_view path, std::size_t offset, std::wstring_view token) noexcept
{
    if (offset > path.size() || token.size() > path.size() - offset) return false;
    for (std::size_t index = 0; index < token.size(); ++index)
        if (FoldAscii(path[offset + index]) != token[index]) return false;
    return true;
}

constexpr bool ReadHex4(std::wstring_view path, std::size_t offset, std::uint16_t& out) noexcept
{
    if (offset > path.size() || 4u > path.size() - offset) return false;
    std::uint16_t value = 0;
    for (std::size_t index = 0; index < 4u; ++index) {
        const wchar_t c = FoldAscii(path[offset + index]);
        const int nibble = c >= L'0' && c <= L'9' ? c - L'0' : c >= L'a' && c <= L'f' ? c - L'a' + 10 : -1;
        if (nibble < 0) return false;
        value = static_cast<std::uint16_t>((value << 4u) | static_cast<std::uint16_t>(nibble));
    }
    out = value;
    return true;
}

// First "vid_XXXX&pid_YYYY" token (case-insensitive), if any.
constexpr bool TryRead(std::wstring_view path, std::uint16_t& vendorId, std::uint16_t& productId) noexcept
{
    constexpr std::size_t kTokenChars = 17u; // vid_XXXX&pid_YYYY
    vendorId = productId = 0;
    if (path.size() < kTokenChars) return false;
    for (std::size_t offset = 0; offset <= path.size() - kTokenChars; ++offset) {
        std::uint16_t vid = 0, pid = 0;
        if (TokenAt(path, offset, L"vid_") && TokenAt(path, offset + 8u, L"&pid_") &&
            ReadHex4(path, offset + 4u, vid) && ReadHex4(path, offset + 13u, pid)) {
            vendorId = vid;
            productId = pid;
            return true;
        }
    }
    return false;
}

// False only when the path proves a different USB identity.
constexpr bool MayBe(std::wstring_view path, std::uint16_t vendorId, std::uint16_t productId) noexcept
{
    std::uint16_t vid = 0, pid = 0;
    return !TryRead(path, vid, pid) || (vid == vendorId && pid == productId);
}

static_assert(MayBe(L"\\\\?\\hid#vid_28e9&pid_3265&mi_02&col01#9&1&0#{4d1e55b2-f16f-11cf-88cb-001111000030}", 0x28E9, 0x3265));
static_assert(MayBe(L"\\\\?\\HID#VID_28E9&PID_3265&MI_02#x", 0x28E9, 0x3265));
static_assert(!MayBe(L"\\\\?\\hid#vid_046d&pid_c547&mi_01#x", 0x28E9, 0x3265));
static_assert(!MayBe(L"\\\\?\\hid#vid_28e9&pid_3266#x", 0x28E9, 0x3265));
static_assert(MayBe(L"\\\\?\\hid#{00001812-0000-1000-8000-00805f9b34fb}_dev_vid&0228e9_pid&3265#x", 0x28E9, 0x3265));
static_assert(MayBe(L"short", 0x28E9, 0x3265));
static_assert(MayBe(L"\\\\?\\hid#vid_zz12&pid_3265#x", 0x28E9, 0x3265)); // unparseable: decide by opening

} // namespace halljoy::usb_path
