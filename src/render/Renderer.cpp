#include "Renderer.h"

Renderer::Renderer(int w, int h) : width(w), height(h) {
    pixels.resize((size_t)width * height);
    initBitmapInfo();
}

Renderer::~Renderer() = default;

void Renderer::initBitmapInfo() {
    bmpInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth       = width;
    bmpInfo.bmiHeader.biHeight      = -height;
    bmpInfo.bmiHeader.biPlanes      = 1;
    bmpInfo.bmiHeader.biBitCount    = 32;
    bmpInfo.bmiHeader.biCompression = BI_RGB;
}

void Renderer::clear() {
    for (auto& px : pixels) px = 0x00101820;
}

void Renderer::drawParticle(const Particle& p) {  // ← убран const
    int cx = (int)p.position.x;
    int cy = (int)p.position.y;

    float maxSpeed = 900.0f;
    float t = p.speed() / maxSpeed;
    if (t > 1.0f) t = 1.0f;

    unsigned char r = (unsigned char)(255.0f * t);
    unsigned char g = 90;
    unsigned char b = (unsigned char)(255.0f * (1.0f - t));
    uint32_t color = 0xFF000000u | (uint32_t)r
                   | ((uint32_t)g << 8) | ((uint32_t)b << 16);

    for (int dy = 0; dy <= 1; ++dy)
        for (int dx = 0; dx <= 1; ++dx) {
            int px = cx + dx, py = cy + dy;
            if (px >= 0 && px < width && py >= 0 && py < height)
                pixels[(size_t)py * width + px] = color;
        }
}

void Renderer::present() const {
    if (!target) return;
    HDC hdc = GetDC(target);
    StretchDIBits(hdc, 0, 0, width, height,
                  0, 0, width, height,
                  pixels.data(), &bmpInfo, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(target, hdc);
}