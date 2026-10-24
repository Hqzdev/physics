#include "Application.h"
#include "BufferedPaint.h"

#include <algorithm>
#include <stdexcept>

Application::Application() { createWindow(); }

Application::~Application() {
    if (window_ && IsWindow(window_)) DestroyWindow(window_);
}

int Application::run() {
    MSG message{};
    int status;
    while ((status = static_cast<int>(GetMessageW(&message, nullptr, 0, 0))) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (status < 0) throw std::runtime_error("Windows message loop failed");
    return static_cast<int>(message.wParam);
}

void Application::recordError(const std::exception& error) {
    paused_ = true;
    const std::string message = error.what();
    error_ = L"Расчёт остановлен: " + std::wstring(message.begin(), message.end()) + L". Home: сброс.";
}

void Application::tick() {
    if (!paused_) {
        try { engine_.advance(stepsPerFrame_); }
        catch (const std::exception& error) { recordError(error); }
    }
    InvalidateRect(window_, nullptr, FALSE);
}

void Application::onKey(WPARAM key) {
    if (key == VK_ESCAPE) { DestroyWindow(window_); return; }
    try {
        if (key >= '1' && key <= '7') {
            engine_.reset(static_cast<physics::Experiment>(key - '1'));
            paused_ = false;
            error_.clear();
        } else if (key == VK_HOME) {
            engine_.reset(engine_.experiment());
            paused_ = false;
            error_.clear();
        } else if (key == VK_SPACE) {
            if (error_.empty()) paused_ = !paused_;
        } else if (key == 'T') {
            engine_.setThermostatEnabled(!engine_.thermostatEnabled());
        } else if (key == 'B') {
            engine_.setBarostatEnabled(!engine_.barostatEnabled());
        } else if (key == VK_UP || key == VK_DOWN) {
            const double factor = key == VK_UP ? 1.1 : 1.0/1.1;
            engine_.setTargetTemperature(engine_.settings().temperatureK*factor);
        } else if (key == VK_LEFT || key == VK_RIGHT) {
            if (engine_.barostatEnabled()) engine_.setTargetPressure(engine_.settings().pressureMPa * (key == VK_RIGHT ? 1.25 : .8));
            else engine_.changeVolume(key == VK_RIGHT ? .97 : 1.0/.97);
        } else if (key == VK_OEM_PLUS || key == VK_ADD) {
            stepsPerFrame_ = std::min(128u, stepsPerFrame_*2);
        } else if (key == VK_OEM_MINUS || key == VK_SUBTRACT) {
            stepsPerFrame_ = std::max(1u, stepsPerFrame_/2);
        }
    } catch (const std::exception& error) { recordError(error); }
    InvalidateRect(window_, nullptr, FALSE);
}

void Application::paint() {
    BufferedPaint paint(window_);
    if (paint.bounds().right <= 0 || paint.bounds().bottom <= 0) return;
    try { renderer_.draw(paint.dc(), paint.bounds(), engine_, paused_, stepsPerFrame_, error_); }
    catch (const std::exception& error) { recordError(error); }
}

LRESULT CALLBACK Application::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* app = reinterpret_cast<Application*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* creation = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<Application*>(creation->lpCreateParams);
        app->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    switch (message) {
    case WM_ERASEBKGND: return 1;
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(lParam)->ptMinTrackSize = {1120, 840};
        return 0;
    case WM_PAINT: if (app) { app->paint(); return 0; } break;
    case WM_TIMER: if (app) { app->tick(); return 0; } break;
    case WM_KEYDOWN: if (app) { app->onKey(wParam); return 0; } break;
    case WM_DESTROY: KillTimer(window, 1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void Application::createWindow() {
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW definition{};
    definition.lpfnWndProc = windowProc;
    definition.hInstance = instance;
    definition.lpszClassName = L"WaterPhysicsEngineWindow";
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassW(&definition) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        throw std::runtime_error("Cannot register window class");
    RECT bounds{0, 0, 1360, 840};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
    window_ = CreateWindowW(definition.lpszClassName, L"Физический движок воды", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top,
        nullptr, nullptr, instance, this);
    if (!window_) throw std::runtime_error("Cannot create simulation window");
    if (!SetTimer(window_, 1, 16, nullptr)) {
        DestroyWindow(window_);
        throw std::runtime_error("Cannot start simulation timer");
    }
    ShowWindow(window_, SW_SHOW);
}
