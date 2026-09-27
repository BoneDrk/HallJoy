#pragma once
#include <gdiplus.h>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <array>
#include <cstring>

// Rasterize axis coverage once, then combine it into a single opaque grid image.
// This avoids hundreds of separately blended GDI+ primitives per camera frame.
struct EditorGrid {
    std::vector<std::uint32_t> pixels;
    std::vector<float> columns, rows;
    std::vector<unsigned char> columnAlpha;
    std::unique_ptr<Gdiplus::Bitmap> bitmap;
    int width = 0, height = 0, step = 0;
    float scale = 0, x = 0, y = 0, opacity = 0;
    static void Axis(std::vector<float>& coverage, int length, float origin,
        float scale, int majorStep, float minorOpacity) {
        coverage.assign(length, 0);
        auto lines = [&](int stride, float thickness, float alpha, bool minor) {
            const float spacing = stride * scale;
            const int first = (int)std::ceil((-1 - origin) / spacing);
            const int last = (int)std::floor((length + 1 - origin) / spacing);
            for (int i = first; i <= last; ++i) {
                if (minor && (i * stride) % majorStep == 0) continue;
                const float center = origin + i * spacing;
                const float lo = center - thickness / 2, hi = center + thickness / 2;
                const int begin = std::max(0, (int)std::floor(lo));
                const int end = std::min(length - 1, (int)std::floor(hi));
                for (int pixel = begin; pixel <= end; ++pixel) {
                    const float amount = std::max(0.0f, std::min(hi, pixel + 1.0f) - std::max(lo, (float)pixel)) * alpha;
                    coverage[pixel] = 1 - (1 - coverage[pixel]) * (1 - amount);
                }
            }
        };
        if (minorOpacity > 0) lines(1, 0.7f, std::round(36 * minorOpacity) / 255.0f, true);
        lines(majorStep, 1.1f, 85.0f / 255.0f, false);
    }
    Gdiplus::Bitmap* Get(int w, int h, float newScale, float originX, float originY,
        int majorStep, float minorOpacity) {
        if (w <= 0 || h <= 0) return nullptr;
        if (bitmap && width == w && height == h && scale == newScale &&
            x == originX && y == originY && step == majorStep && opacity == minorOpacity) return bitmap.get();
        if (!bitmap || width != w || height != h) {
            bitmap.reset(); pixels.resize(static_cast<size_t>(w) * h);
            bitmap = std::make_unique<Gdiplus::Bitmap>(w, h, w * 4, PixelFormat32bppRGB,
                reinterpret_cast<BYTE*>(pixels.data()));
        }
        width = w; height = h; scale = newScale; x = originX; y = originY; step = majorStep; opacity = minorOpacity;
        Axis(columns, w, originX, scale, step, opacity);
        Axis(rows, h, originY, scale, step, opacity);
        static const auto colors = [] {
            std::array<std::uint32_t, 65536> table{};
            for (int ay = 0; ay < 256; ++ay) for (int ax = 0; ax < 256; ++ax) {
                const float a = 1 - (1 - ax/255.0f) * (1 - ay/255.0f);
                const auto rg = static_cast<std::uint32_t>(28 + 72*a + 0.5f);
                const auto b = static_cast<std::uint32_t>(30 + 80*a + 0.5f);
                table[ay*256+ax] = 0xFF000000u | (rg << 16) | (rg << 8) | b;
            }
            return table;
        }();
        columnAlpha.resize(w);
        for (int col = 0; col < w; ++col) columnAlpha[col] = static_cast<unsigned char>(std::lround(columns[col]*255));
        std::array<int, 256> firstRow; firstRow.fill(-1);
        for (int row = 0; row < h; ++row) {
            const int alpha = (int)std::lround(rows[row]*255);
            auto* destination = pixels.data() + static_cast<size_t>(row)*w;
            if (firstRow[alpha] >= 0)
                std::memcpy(destination, pixels.data() + static_cast<size_t>(firstRow[alpha])*w, w*sizeof(std::uint32_t));
            else {
                firstRow[alpha] = row;
                for (int col = 0; col < w; ++col) destination[col] = colors[alpha*256 + columnAlpha[col]];
            }
        }
        return bitmap.get();
    }
};
