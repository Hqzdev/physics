#pragma once

#include <cstddef>
#include <windows.h>
#include "../physics/thermo/WaterPhaseModel.h"
#include "../physics/thermo/WaterRegimeCatalog.h"

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
    WaterPhaseModel waterModel_;
    double temperatureK_ = 373.15;
    double densityKgM3_ = 50.0;
    std::size_t selectedRegimeIndex_ = 0;

    void createWindow();
    void updateDashboard();
    void drawDashboard(HDC deviceContext) const;
    void drawRegimeCatalog(HDC deviceContext) const;
};
