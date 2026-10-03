#pragma once
// One Direct2D surface that draws the whole keyboard view.
//
// The key buttons stay as input-only child windows (hit testing, capture,
// drag, cursor). They draw nothing; the canvas sits above them, is transparent
// to the mouse (HTTRANSPARENT) and renders every key in one pass. Frames are
// paced to the desktop compositor: any number of change requests between two
// display refreshes produce one frame.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d2d1.h>

namespace halljoy::keyboard_canvas {

// Process-wide Direct2D factory (multi-threaded; drawing stays on the UI thread).
ID2D1Factory* Factory();
// Starts a worker that pays the first-device cost (D3D driver initialisation,
// DirectWrite system fonts) before the UI needs it. Call once at startup;
// JoinWarmUp before process exit (bounded wait).
void WarmUpAsync();
// Bounded wait used right before the main window is first shown, so it
// appears once with the keyboard drawn instead of an unpainted canvas.
// Messages sent by other threads and the caller's serve message keep flowing.
// serveWindow/serveMessage: one posted message kept flowing during the wait.
bool WaitForWarmUp(DWORD timeoutMs, HWND serveWindow = nullptr, UINT serveMessage = 0);
void JoinWarmUp();

// Draws the full scene. rt is in canvas client coordinates (DIPs == pixels).
// Returns true while an animation still needs the next frame.
using PaintCallback = bool (*)(HWND canvas, ID2D1RenderTarget* rt, void* context);

// Creates the canvas as a child of parent. Returns nullptr on failure; the
// caller then keeps the classic per-key painting.
HWND Create(HWND parent, PaintCallback paint, void* context);
void Destroy(HWND canvas);

// Requests one frame. Cheap and safe to call at any rate from the UI thread.
void RequestFrame(HWND canvas);

// Places the canvas and keeps it directly above the given input windows
// (all other siblings stay above it).
void Place(HWND canvas, const RECT& boundsInParent);
// Puts the canvas at the bottom of its siblings, then the input windows below it.
void KeepAbove(HWND canvas, HWND inputWindow);
void SendToBottom(HWND canvas);

} // namespace halljoy::keyboard_canvas
