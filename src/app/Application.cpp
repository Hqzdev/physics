#include "Application.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace {
using watermd::Vec3;

std::wstring formatValue(double value, int decimals = 2) {
    std::wostringstream output;
    output << std::fixed << std::setprecision(decimals) << value;
    return output.str();
}

void drawText(HDC dc, int x, int y, int width, int height,
              const std::wstring& value, COLORREF color,
              UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    SetTextColor(dc, color);
    RECT bounds{x, y, x + width, y + height};
    DrawTextW(dc, value.c_str(), -1, &bounds, format);
}

void drawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF color, int width = 1) {
    HPEN pen = CreatePen(PS_SOLID, width, color);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    MoveToEx(dc, x1, y1, nullptr);
    LineTo(dc, x2, y2);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void drawAtom(HDC dc, int x, int y, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, x - radius, y - radius, x + radius + 1, y + radius + 1);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

Vec3 minimumImage(Vec3 displacement, double boxLengthA) {
    displacement.x -= boxLengthA * std::round(displacement.x / boxLengthA);
    displacement.y -= boxLengthA * std::round(displacement.y / boxLengthA);
    displacement.z -= boxLengthA * std::round(displacement.z / boxLengthA);
    return displacement;
}

std::wstring observedStructure(const watermd::Measurements& measurements) {
    if (measurements.averageOxygenNeighbors < 1.5) {
        return L"Разреженная газоподобная структура";
    }
    if (measurements.tetrahedralOrder > 0.78) {
        return L"Упорядоченная структура, похожая на лёд";
    }
    return L"Плотная неупорядоченная структура, похожая на жидкость";
}
}

Application::Application() {
    createWindow();
    updateDashboard();
}

int Application::run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void Application::updateDashboard() {
    std::wstring title = L"Движение молекул воды | T=";
    title += formatValue(targetTemperatureK_, 1);
    title += L" K, P=" + formatValue(targetPressureMPa_, 3) + L" MPa";
    SetWindowTextW(window_, title.c_str());
    InvalidateRect(window_, nullptr, FALSE);
}

void Application::drawDashboard(HDC dc) const {
    RECT client{};
    GetClientRect(window_, &client);
    HBRUSH background = CreateSolidBrush(RGB(12, 18, 29));
    FillRect(dc, &client, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);

    const int boxLeft = 52;
    const int boxTop = 92;
    const int boxSide = (std::min)(480, client.bottom - 190);
    const int panelLeft = boxLeft + static_cast<int>(boxSide * 1.32) + 45;
    const int panelWidth = (std::max)(250, client.right - panelLeft - 35);
    const double boxLength = waterSimulation_.boxLengthA();

    drawText(dc, 48, 24, client.right - 96, 42,
        L"Молекулы воды: движение из сил между атомами",
        RGB(240, 245, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Показываем кубическую периодическую ячейку как 3D-проекцию.
    auto project = [&](const Vec3& position) {
        const double px = position.x / boxLength + 0.25 * position.z / boxLength;
        const double py = position.y / boxLength + 0.18 * position.z / boxLength;
        return POINT{
            boxLeft + static_cast<int>(px * boxSide),
            boxTop + static_cast<int>(py * boxSide)
        };
    };

    std::array<Vec3, 8> corners{};
    for (int i = 0; i < 8; ++i) {
        corners[static_cast<std::size_t>(i)] = {
            (i & 1) ? boxLength : 0.0,
            (i & 2) ? boxLength : 0.0,
            (i & 4) ? boxLength : 0.0
        };
    }
    for (int i = 0; i < 8; ++i) {
        for (int axis = 0; axis < 3; ++axis) {
            const int j = i ^ (1 << axis);
            if (j > i) {
                const POINT a = project(corners[static_cast<std::size_t>(i)]);
                const POINT b = project(corners[static_cast<std::size_t>(j)]);
                drawLine(dc, a.x, a.y, b.x, b.y, RGB(65, 82, 105));
            }
        }
    }

    const auto& molecules = waterSimulation_.molecules();
    for (const watermd::WaterMolecule& molecule : molecules) {
        const Vec3 oxygen = molecule.atoms[0].positionA;
        for (std::size_t hydrogenIndex = 1; hydrogenIndex < 3; ++hydrogenIndex) {
            const Vec3 bond = minimumImage(
                molecule.atoms[hydrogenIndex].positionA - oxygen, boxLength);
            const POINT oxygenPoint = project(oxygen);
            const POINT hydrogenPoint = project(oxygen + bond);
            drawLine(dc, oxygenPoint.x, oxygenPoint.y,
                hydrogenPoint.x, hydrogenPoint.y, RGB(96, 125, 157), 2);
            drawAtom(dc, hydrogenPoint.x, hydrogenPoint.y, 3, RGB(195, 222, 255));
        }

        const POINT oxygenPoint = project(oxygen);
        drawAtom(dc, oxygenPoint.x, oxygenPoint.y, 6, RGB(72, 190, 255));
    }

    const watermd::Measurements& measured = waterSimulation_.measurements();
    drawText(dc, panelLeft, 94, panelWidth, 34,
        L"Условия системы", RGB(240, 245, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, panelLeft, 138, panelWidth, 28,
        L"Целевая температура: " + formatValue(targetTemperatureK_, 1) + L" K",
        RGB(205, 220, 240));
    drawText(dc, panelLeft, 172, panelWidth, 28,
        L"Измеренная температура: " + formatValue(measured.temperatureK, 1) + L" K",
        RGB(205, 220, 240));
    drawText(dc, panelLeft, 206, panelWidth, 28,
        L"Целевое давление: " + formatValue(targetPressureMPa_, 3) + L" МПа",
        RGB(205, 220, 240));
    drawText(dc, panelLeft, 240, panelWidth, 28,
        L"Измеренное давление: " + formatValue(measured.pressureMPa, 3) + L" МПа",
        RGB(205, 220, 240));
    drawText(dc, panelLeft, 274, panelWidth, 28,
        L"Плотность: " + formatValue(measured.densityKgM3, 1) + L" кг/м³",
        RGB(205, 220, 240));

    drawText(dc, panelLeft, 326, panelWidth, 32,
        L"Что видно по атомам", RGB(240, 245, 255), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, panelLeft, 366, panelWidth, 60,
        observedStructure(measured), RGB(111, 222, 180),
        DT_LEFT | DT_WORDBREAK | DT_TOP);
    drawText(dc, panelLeft, 432, panelWidth, 52,
        L"Тетраэдрический порядок: " + formatValue(measured.tetrahedralOrder, 3),
        RGB(205, 220, 240), DT_LEFT | DT_WORDBREAK | DT_TOP);
    drawText(dc, panelLeft, 482, panelWidth, 48,
        L"Среднее число соседних атомов O: " +
            formatValue(measured.averageOxygenNeighbors, 2),
        RGB(205, 220, 240), DT_LEFT | DT_WORDBREAK | DT_TOP);

    drawText(dc, panelLeft, 548, panelWidth, 78,
        L"Стрелки вверх/вниз: температура. Вправо/влево: давление. Home: сброс. "
        L"Атомы движутся по 3D-модели типа SPC/Fw; окно показывает проекцию. "
        L"Это приближение: электростатика обрезана на радиусе взаимодействия.",
        RGB(151, 169, 195), DT_LEFT | DT_WORDBREAK | DT_TOP);
    drawText(dc, 48, client.bottom - 34, client.right - 96, 24,
        L"Расчёт классический: он показывает молекулярную воду, но не распад молекул, плазму, БЭК или ферми-вырождение.",
        RGB(125, 143, 169), DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

LRESULT CALLBACK Application::windowProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    Application* app = reinterpret_cast<Application*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        if (app) {
            app->drawDashboard(dc);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_TIMER:
        if (app) {
            app->waterSimulation_.advance(24);
            InvalidateRect(window, nullptr, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (!app) {
            break;
        }
        if (wParam == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        if (wParam == VK_UP) {
            app->targetTemperatureK_ = (std::min)(
                app->targetTemperatureK_ + 10.0, 600.0);
        } else if (wParam == VK_DOWN) {
            app->targetTemperatureK_ = (std::max)(
                app->targetTemperatureK_ - 10.0, 80.0);
        } else if (wParam == VK_RIGHT) {
            app->targetPressureMPa_ = (std::min)(
                app->targetPressureMPa_ * 1.25, 100.0);
        } else if (wParam == VK_LEFT) {
            app->targetPressureMPa_ = (std::max)(
                app->targetPressureMPa_ / 1.25, 0.001);
        } else if (wParam == VK_HOME) {
            app->targetTemperatureK_ = 300.0;
            app->targetPressureMPa_ = 0.1;
        } else {
            return 0;
        }
        app->waterSimulation_.setTargetTemperature(app->targetTemperatureK_);
        app->waterSimulation_.setTargetPressure(app->targetPressureMPa_);
        app->updateDashboard();
        return 0;
    case WM_DESTROY:
        KillTimer(window, 1);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

void Application::createWindow() {
    HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = windowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = L"WaterMolecularDynamicsWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(
        GetStockObject(BLACK_BRUSH));
    RegisterClassW(&windowClass);

    RECT bounds{0, 0, WIDTH, HEIGHT};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowW(
        windowClass.lpszClassName,
        L"Движение молекул воды",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        instance,
        nullptr);

    SetWindowLongPtrW(window_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    SetTimer(window_, 1, 16, nullptr);
    ShowWindow(window_, SW_SHOW);
}
