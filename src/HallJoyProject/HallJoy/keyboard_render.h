#pragma once
#include <windows.h>
#include <d2d1.h>
#include <cstdint>
#include "binding_actions.h"

// reads analog with key-specific deadzones + global fallback
float KeyboardRender_ReadAnalog01(uint16_t hid);

// draws one key (owner-draw button) through a Direct2D DC render target
// v01 can be a cached value (recommended). If v01 < 0 -> function will read itself.
void KeyboardRender_DrawKey(const DRAWITEMSTRUCT* dis, uint16_t hid, bool selected, float v01);

// One key for a Direct2D target (the keyboard canvas). rc is in target pixels.
struct KeyboardRenderKey
{
    HWND dpiWindow = nullptr;  // window used for DPI scaling of marks and text
    UINT dpi = 0;              // that window's DPI if already known (0 = query it)
    RECT rc{};
    POINT notch{};             // KeyShape_Get of the key window
    uint16_t hid = 0;          // HID whose analog value and icons are shown
    uint16_t actualHid = 0;    // HID stored on the key window (selection, gear)
    const wchar_t* label = nullptr;
    bool selected = false;
    bool disabled = false;
    bool dropHover = false;
    float v01 = -1.0f;         // < 0 reads the current analog value
};
void KeyboardRender_DrawKeyD2D(ID2D1RenderTarget* rt, const KeyboardRenderKey& key);
// Centered single-line text in the key font family.
void KeyboardRender_DrawTextD2D(ID2D1RenderTarget* rt, const wchar_t* text, const RECT& area, COLORREF color,
    float sizePx);
// True while a selection, flash, or gear animation needs further frames.
bool KeyboardRender_AnyAnimationActive();
// Releases brushes and bitmaps created for rt (call before rt is destroyed).
void KeyboardRender_ReleaseTargetResources(ID2D1RenderTarget* rt);

// -----------------------------------------------------------------------------
// NEW: gear animation support
// -----------------------------------------------------------------------------
// Returns list of HID codes (<=255) that currently need redraw for gear animation.
// Called from UI timer tick to invalidate only needed keys.
//
// outHids: buffer to write HIDs
// cap: buffer capacity
// returns: number of HIDs written
int KeyboardRender_GetAnimatingHids(uint16_t* outHids, int cap);

// -----------------------------------------------------------------------------
// NEW: gear "wow spin" interaction
// -----------------------------------------------------------------------------
//
// - Called by UI when selection changes on Configuration tab.
//   Used to stop spinning when user stops editing that key.
void KeyboardRender_NotifySelectedHid(uint16_t hid);

// - Called by UI when user clicks the gear marker on a key that has Override enabled.
//   Starts a smooth "fast burst -> slow idle spin" while the key remains selected/edited.
void KeyboardRender_OnGearClicked(uint16_t hid);

// Temporarily suppress one specific bound icon while drawing key(s), used by drag UX.
// Call KeyboardRender_ClearSuppressedBinding() after drawing.
void KeyboardRender_SetSuppressedBinding(uint16_t hid, int padIndex, BindAction action);
void KeyboardRender_ClearSuppressedBinding();
