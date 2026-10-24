#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "../physics/SimulationEngine.h"
#include "SimulationRenderer.h"
#include <exception>
#include <string>

// Связывает события Windows, движок и отрисовку. Физики внутри окна нет.
class Application {
public:
    Application();
    ~Application();
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    int run();
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
private:
    HWND window_ = nullptr;
    physics::SimulationEngine engine_;
    SimulationRenderer renderer_;
    bool paused_ = false;
    unsigned int stepsPerFrame_ = 8;
    std::wstring error_;
    void createWindow();
    void tick();
    void onKey(WPARAM key);
    void paint();
    void recordError(const std::exception& error);
};
