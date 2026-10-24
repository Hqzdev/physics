#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../physics/SimulationEngine.h"
#include <string>

// Только GDI-отрисовка снимка расчёта: камера, частицы, легенда, измерения.
class SimulationRenderer {
public:
    void draw(HDC dc, const RECT& client, const physics::SimulationEngine& engine,
              bool paused, unsigned int stepsPerFrame, const std::wstring& error) const;
private:
    void drawParticles(HDC dc, const physics::SimulationEngine& engine, int left, int top, int side) const;
    void drawMeasurements(HDC dc, const physics::SimulationEngine& engine, int left, int width) const;
};
