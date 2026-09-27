#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <memory>
#include <cstdint>

// One top-down pixel buffer per editor, reused until its client size changes.
struct EditorRenderBuffer {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    int width = 0, height = 0;
    std::uint32_t* pixels = nullptr;
    std::unique_ptr<Gdiplus::Bitmap> surface;
    ~EditorRenderBuffer() { Reset(); }
    void Reset() {
        surface.reset();
        if (dc && previous) SelectObject(dc, previous);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        dc = nullptr; bitmap = nullptr; previous = nullptr; width = height = 0;
        pixels = nullptr;
    }
    bool Ensure(int w, int h) {
        if (dc && width == w && height == h) return true;
        Reset();
        if (w <= 0 || h <= 0) return false;
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = w; info.bmiHeader.biHeight = -h;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void* bits = nullptr;
        dc = CreateCompatibleDC(nullptr);
        bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        pixels = static_cast<std::uint32_t*>(bits);
        if (!dc || !bitmap || !pixels) { Reset(); return false; }
        previous = SelectObject(dc, bitmap);
        surface = std::make_unique<Gdiplus::Bitmap>(w, h, w * 4, PixelFormat32bppRGB, reinterpret_cast<BYTE*>(pixels));
        if (surface->GetLastStatus() != Gdiplus::Ok) { Reset(); return false; }
        width = w; height = h;
        return true;
    }
};
