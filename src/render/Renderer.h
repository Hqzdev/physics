#pragma once
#include <windows.h>
#include <vector>
#include <cstdint>
#include "../physics/ParticleSystem.h"

class Renderer {
public:
    Renderer(int w, int h);
    ~Renderer();

    void clear();
    void drawParticle(const Particle& p);  // ← убран const
    void present() const;

    void attachWindow(HWND hwnd) { target = hwnd; }

private:
    int width, height;
    HWND  target = nullptr;
    std::vector<uint32_t> pixels;
    BITMAPINFO bmpInfo{};

    void initBitmapInfo();
};