#pragma once

#include <cstdint>

struct D3DXCOLOR {
    float r, g, b, a;
    
    D3DXCOLOR() : r(1), g(1), b(1), a(1) {}
    D3DXCOLOR(float red, float green, float blue, float alpha = 1.0f)
        : r(red), g(green), b(blue), a(alpha) {}
    D3DXCOLOR(uint32_t argb) {
        b = ((argb >> 16) & 0xFF) / 255.0f;
        g = ((argb >> 8) & 0xFF) / 255.0f;
        r = (argb & 0xFF) / 255.0f;
        a = ((argb >> 24) & 0xFF) / 255.0f;
    }
};
