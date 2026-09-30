#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <string>
#include "input_shortcuts.h"

// With Num Lock on, Windows wraps Shift + numpad key in synthesized Shift
// release/press events carrying an E0 prefix (E0 2A / E0 36). No physical Shift
// key sends E0, and the real Shift stays held: these must not change key state,
// or Ctrl+Shift+Numpad shortcuts would never match on the digital path.
inline bool IsSyntheticNumpadShift(DWORD scanCode, bool extended)
{
    const DWORD scan = scanCode & 0xFFu;
    return extended && (scan == 0x2A || scan == 0x36);
}

// Physical key identity shared by the low-level hook, Raw Input and the
// shortcut UI: PC/AT set-1 scan code (+E0) -> USB HID keyboard usage.
inline std::uint16_t HidFromKeyboardScanCode(DWORD scanCode, bool extended, DWORD vkCode)
{
    if (vkCode == VK_PAUSE) return 72;
    if (vkCode >= VK_F13 && vkCode <= VK_F24)
        return static_cast<std::uint16_t>(104 + vkCode - VK_F13);
    switch (scanCode & 0xFFu)
    {
    case 0x01: return 41; // Esc
    case 0x02: return 30; // 1
    case 0x03: return 31; // 2
    case 0x04: return 32; // 3
    case 0x05: return 33; // 4
    case 0x06: return 34; // 5
    case 0x07: return 35; // 6
    case 0x08: return 36; // 7
    case 0x09: return 37; // 8
    case 0x0A: return 38; // 9
    case 0x0B: return 39; // 0
    case 0x0C: return 45; // -
    case 0x0D: return 46; // =
    case 0x0E: return 42; // Backspace
    case 0x0F: return 43; // Tab
    case 0x10: return 20; // Q
    case 0x11: return 26; // W
    case 0x12: return 8;  // E
    case 0x13: return 21; // R
    case 0x14: return 23; // T
    case 0x15: return 28; // Y
    case 0x16: return 24; // U
    case 0x17: return 12; // I
    case 0x18: return 18; // O
    case 0x19: return 19; // P
    case 0x1A: return 47; // [
    case 0x1B: return 48; // ]
    case 0x1C: return extended ? 88 : 40; // Enter / Numpad Enter
    case 0x1D: return extended ? 228 : 224; // RCtrl / LCtrl
    case 0x1E: return 4;  // A
    case 0x1F: return 22; // S
    case 0x20: return 7;  // D
    case 0x21: return 9;  // F
    case 0x22: return 10; // G
    case 0x23: return 11; // H
    case 0x24: return 13; // J
    case 0x25: return 14; // K
    case 0x26: return 15; // L
    case 0x27: return 51; // ;
    case 0x28: return 52; // '
    case 0x29: return 53; // `
    case 0x2A: return 225; // LShift
    case 0x2B: return 49; // Backslash
    case 0x2C: return 29; // Z
    case 0x2D: return 27; // X
    case 0x2E: return 6;  // C
    case 0x2F: return 25; // V
    case 0x30: return 5;  // B
    case 0x31: return 17; // N
    case 0x32: return 16; // M
    case 0x33: return 54; // ,
    case 0x34: return 55; // .
    case 0x35: return extended ? 84 : 56; // Numpad / or /
    case 0x36: return 229; // RShift
    case 0x37: return extended ? 70 : 85; // PrintScreen / Numpad *
    case 0x38: return extended ? 230 : 226; // RAlt / LAlt
    case 0x39: return 44; // Space
    case 0x3A: return 57; // CapsLock
    case 0x3B: return 58; // F1
    case 0x3C: return 59; // F2
    case 0x3D: return 60; // F3
    case 0x3E: return 61; // F4
    case 0x3F: return 62; // F5
    case 0x40: return 63; // F6
    case 0x41: return 64; // F7
    case 0x42: return 65; // F8
    case 0x43: return 66; // F9
    case 0x44: return 67; // F10
    case 0x45: return 83; // NumLock
    case 0x46: return 71; // ScrollLock
    case 0x47: return extended ? 74 : 95; // Home / Numpad 7
    case 0x48: return extended ? 82 : 96; // Up / Numpad 8
    case 0x49: return extended ? 75 : 97; // PgUp / Numpad 9
    case 0x4A: return 86; // Numpad -
    case 0x4B: return extended ? 80 : 92; // Left / Numpad 4
    case 0x4C: return 93; // Numpad 5
    case 0x4D: return extended ? 79 : 94; // Right / Numpad 6
    case 0x4E: return 87; // Numpad +
    case 0x4F: return extended ? 77 : 89; // End / Numpad 1
    case 0x50: return extended ? 81 : 90; // Down / Numpad 2
    case 0x51: return extended ? 78 : 91; // PgDn / Numpad 3
    case 0x52: return extended ? 73 : 98; // Insert / Numpad 0
    case 0x53: return extended ? 76 : 99; // Delete / Numpad .
    case 0x56: return 100; // ISO extra key (non-US backslash)
    case 0x57: return 68; // F11
    case 0x58: return 69; // F12
    case 0x5B: return 227; // LWin
    case 0x5C: return 231; // RWin
    case 0x5D: return 101; // Menu/App
    default:
        break;
    }

    // Fallback for rare events with zero/unknown scan code.
    switch (vkCode)
    {
    case 'A': return 4; case 'B': return 5; case 'C': return 6; case 'D': return 7; case 'E': return 8;
    case 'F': return 9; case 'G': return 10; case 'H': return 11; case 'I': return 12; case 'J': return 13;
    case 'K': return 14; case 'L': return 15; case 'M': return 16; case 'N': return 17; case 'O': return 18;
    case 'P': return 19; case 'Q': return 20; case 'R': return 21; case 'S': return 22; case 'T': return 23;
    case 'U': return 24; case 'V': return 25; case 'W': return 26; case 'X': return 27; case 'Y': return 28;
    case 'Z': return 29;
    case '1': return 30; case '2': return 31; case '3': return 32; case '4': return 33; case '5': return 34;
    case '6': return 35; case '7': return 36; case '8': return 37; case '9': return 38; case '0': return 39;
    case VK_SPACE: return 44;
    case VK_TAB: return 43;
    case VK_RETURN: return extended ? 88 : 40;
    case VK_BACK: return 42;
    case VK_ESCAPE: return 41;
    case VK_LEFT: return 80;
    case VK_RIGHT: return 79;
    case VK_UP: return 82;
    case VK_DOWN: return 81;
    case VK_HOME: return 74;
    case VK_END: return 77;
    case VK_PRIOR: return 75;
    case VK_NEXT: return 78;
    case VK_INSERT: return 73;
    case VK_DELETE: return 76;
    default:
        return 0;
    }
}

// Legacy Block Bound Keys setting stored a virtual-key chord: vk | MOD_* << 8.
// Keypad digits were normalized to VK_NUMPADn independently of Num Lock.
// Low-level hook events: remote-desktop and automation tools may inject a key
// with only a virtual key (scan code 0). Derive the physical position from it.
inline std::uint16_t HidFromHookKey(DWORD scanCode, bool extended, DWORD vkCode)
{
    if ((scanCode & 0xFFu) == 0 && vkCode) {
        const UINT scan = MapVirtualKeyW(vkCode, MAPVK_VK_TO_VSC_EX);
        scanCode = scan & 0xFFu;
        // MapVirtualKey does not always report the E0 prefix.
        switch (vkCode) {
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_HOME: case VK_END:
        case VK_PRIOR: case VK_NEXT: case VK_INSERT: case VK_DELETE: case VK_RCONTROL:
        case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS: case VK_DIVIDE: case VK_SNAPSHOT:
            extended = true; break;
        default:
            extended = extended || (scan & 0xFF00u) == 0xE000u; break;
        }
    }
    return HidFromKeyboardScanCode(scanCode, extended, vkCode);
}

inline unsigned ShortcutFromLegacyVkChord(unsigned chord)
{
    if (!chord || chord > 4095) return 0;
    const unsigned vk = chord & 255, mods = (chord >> 8) & 15;
    static constexpr unsigned kKeypadScan[] = {0x52,0x4f,0x50,0x51,0x4b,0x4c,0x4d,0x47,0x48,0x49};
    unsigned hid = 0;
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) hid = HidFromKeyboardScanCode(kKeypadScan[vk - VK_NUMPAD0], false, 0);
    else if (vk == VK_DECIMAL) hid = HidFromKeyboardScanCode(0x53, false, 0);
    else {
        const UINT scan = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
        const bool extended = (scan & 0xff00) == 0xe000 ||
            (vk >= VK_PRIOR && vk <= VK_DOWN) || vk == VK_INSERT || vk == VK_DELETE;
        hid = HidFromKeyboardScanCode(scan & 255, extended, vk);
    }
    const unsigned shortcut = halljoy::shortcuts::Make(hid, mods);
    return hid && halljoy::shortcuts::Valid(shortcut) ? shortcut : 0;
}

// Human-readable key name for a HID usage, using the active Windows layout.
inline std::wstring HidKeyName(unsigned hid)
{
    if (hid >= 104 && hid <= 115) return L"F" + std::to_wstring(hid - 91);
    if (hid == 72) return L"Pause";
    static const wchar_t* const kModifiers[] = {L"Left Ctrl", L"Left Shift", L"Left Alt", L"Left Win",
        L"Right Ctrl", L"Right Shift", L"Right Alt", L"Right Win"};
    if (halljoy::shortcuts::IsModifier(hid)) return kModifiers[hid - 224];
    for (unsigned extended = 0; extended < 2; ++extended) {
        for (unsigned scan = 1; scan < 0x80; ++scan) {
            if (HidFromKeyboardScanCode(scan, extended != 0, 0) != hid) continue;
            // The table maps some scans identically with and without E0; prefer
            // the first match, which is the plain key.
            wchar_t name[64]{};
            const LONG flags = static_cast<LONG>((scan << 16) | (extended ? (1u << 24) : 0u));
            if (GetKeyNameTextW(flags, name, 64) > 0) {
                std::wstring text(name);
                if (hid >= 84 && hid <= 99 && text.rfind(L"Num", 0) != 0) text = L"Num " + text;
                return text;
            }
        }
    }
    return L"Key " + std::to_wstring(hid);
}

inline std::wstring ShortcutText(unsigned shortcut)
{
    if (!shortcut) return L"Not assigned";
    std::wstring text;
    const unsigned mods = halljoy::shortcuts::Mods(shortcut);
    if (mods & halljoy::shortcuts::kCtrl) text += L"Ctrl + ";
    if (mods & halljoy::shortcuts::kAlt) text += L"Alt + ";
    if (mods & halljoy::shortcuts::kShift) text += L"Shift + ";
    if (mods & halljoy::shortcuts::kWin) text += L"Win + ";
    return text + HidKeyName(halljoy::shortcuts::Key(shortcut));
}
