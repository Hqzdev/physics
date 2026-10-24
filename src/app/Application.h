#pragma once

// Запрещает Windows.h объявлять макросы min и max,
// которые мешают вызовам std::min и std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../physics/md/WaterMolecularDynamics.h"

// Оконное приложение для исследования равновесия воды.
class Application {
public:
    static constexpr int WIDTH = 1280;
    static constexpr int HEIGHT = 720;

    Application();
    int run();

    static LRESULT CALLBACK windowProc(
        HWND window,
        UINT message,
        WPARAM wParam,
        LPARAM lParam);

private:
    HWND window_ = nullptr;
    watermd::WaterMolecularDynamics waterSimulation_;
    double targetTemperatureK_ = 300.0;
    double targetPressureMPa_ = 0.1;

    void createWindow();
    void updateDashboard();
    void drawDashboard(HDC deviceContext) const;
};
