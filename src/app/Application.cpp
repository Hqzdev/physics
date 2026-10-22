#include "Application.h"

#include <algorithm>
#include <string>

namespace {
const wchar_t* phaseLabel(WaterPhaseModel::Region region) {
    switch (region) {
    case WaterPhaseModel::Region::LiquidVaporMixture:
        return L"Жидкость и пар";
    case WaterPhaseModel::Region::SaturatedLiquid:
        return L"Насыщенная жидкость";
    case WaterPhaseModel::Region::SaturatedVapor:
        return L"Насыщенный пар";
    case WaterPhaseModel::Region::OutsideLiquidVaporCoexistence:
        return L"Вне области сосуществования";
    case WaterPhaseModel::Region::TriplePoint:
        return L"Тройная точка: лёд, жидкость и пар";
    case WaterPhaseModel::Region::CriticalPoint:
        return L"Критическая точка";
    case WaterPhaseModel::Region::Unsupported:
    default:
        return L"Условия вне модели";
    }
}

COLORREF phaseColor(WaterPhaseModel::Region region) {
    switch (region) {
    case WaterPhaseModel::Region::LiquidVaporMixture:
    case WaterPhaseModel::Region::SaturatedLiquid:
    case WaterPhaseModel::Region::SaturatedVapor:
        return RGB(90, 210, 240);
    case WaterPhaseModel::Region::TriplePoint:
        return RGB(255, 205, 100);
    case WaterPhaseModel::Region::CriticalPoint:
        return RGB(200, 150, 255);
    default:
        return RGB(255, 130, 120);
    }
}

void drawText(HDC dc, int x, int y, const std::wstring& value,
              COLORREF color, int height = 28) {
    SetTextColor(dc, color);
    RECT bounds{x, y, 1180, y + height};
    DrawTextW(dc, value.c_str(), -1, &bounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

void drawWrappedText(HDC dc, int x, int y, int width, int height,
                     const std::wstring& value, COLORREF color) {
    SetTextColor(dc, color);
    RECT bounds{x, y, x + width, y + height};
    DrawTextW(dc, value.c_str(), -1, &bounds, DT_LEFT | DT_WORDBREAK);
}

std::wstring fixedValue(double value, int decimals = 2) {
    const std::wstring text = std::to_wstring(value);
    const std::size_t decimalPoint = text.find(L'.');
    if (decimalPoint == std::wstring::npos) {
        return text;
    }
    return text.substr(0, decimalPoint + decimals + 1);
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
    const WaterPhaseModel::Result state =
        waterModel_.evaluate(temperatureK_, densityKgM3_);

    std::wstring title = L"Вода: ";
    title += phaseLabel(state.region);
    title += L" | T=" + fixedValue(temperatureK_) + L" K";
    SetWindowTextW(window_, title.c_str());
    InvalidateRect(window_, nullptr, TRUE);
}

void Application::drawDashboard(HDC dc) const {
    const WaterPhaseModel::Result state =
        waterModel_.evaluate(temperatureK_, densityKgM3_);

    RECT client{};
    GetClientRect(window_, &client);
    HBRUSH background = CreateSolidBrush(RGB(15, 21, 32));
    FillRect(dc, &client, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);

    drawText(dc, 48, 34, L"Состояние воды", RGB(240, 245, 255), 42);
    drawText(dc, 48, 100, phaseLabel(state.region), phaseColor(state.region), 36);

    const double temperatureC = temperatureK_ - 273.15;
    drawText(dc, 48, 170,
        L"Температура: " + fixedValue(temperatureK_) + L" K  (" +
        fixedValue(temperatureC) + L" °C)",
        RGB(220, 230, 245));
    drawText(dc, 48, 212,
        L"Общая плотность: " + fixedValue(densityKgM3_) + L" кг/м³",
        RGB(220, 230, 245));

    if (state.pressureAvailable) {
        drawText(dc, 48, 254,
            L"Давление насыщения: " + fixedValue(state.pressurePa / 1.0e6, 4) +
            L" МПа",
            RGB(220, 230, 245));
    } else {
        drawText(dc, 48, 254,
            L"Давление: эта модель его не рассчитывает для однофазного состояния",
            RGB(255, 180, 150));
    }

    if (state.region == WaterPhaseModel::Region::LiquidVaporMixture) {
        drawText(dc, 48, 296,
            L"Насыщенная жидкость: " + fixedValue(state.liquidDensityKgM3) +
            L" кг/м³",
            RGB(140, 200, 255));
        drawText(dc, 48, 338,
            L"Насыщенный пар: " + fixedValue(state.vaporDensityKgM3, 4) +
            L" кг/м³",
            RGB(255, 190, 120));
        drawText(dc, 48, 380,
            L"Доля пара по массе: " +
            fixedValue(state.vaporMassFraction * 100.0, 3) + L"%",
            RGB(220, 230, 245));

        RECT bar{48, 430, 948, 454};
        HBRUSH liquidBrush = CreateSolidBrush(RGB(70, 145, 220));
        FillRect(dc, &bar, liquidBrush);
        DeleteObject(liquidBrush);

        const int vaporWidth = static_cast<int>(
            (bar.right - bar.left) * state.vaporMassFraction);
        RECT vaporBar{bar.right - vaporWidth, bar.top, bar.right, bar.bottom};
        HBRUSH vaporBrush = CreateSolidBrush(RGB(245, 160, 75));
        FillRect(dc, &vaporBar, vaporBrush);
        DeleteObject(vaporBrush);
    } else if (state.region == WaterPhaseModel::Region::TriplePoint) {
        drawText(dc, 48, 296,
            L"В тройной точке могут сосуществовать три фазы. "
            L"Их доли эта модель не рассчитывает.",
            RGB(230, 220, 200));
    } else if (state.region == WaterPhaseModel::Region::CriticalPoint) {
        drawText(dc, 48, 296,
            L"Плотность в критической точке: 322 кг/м³",
            RGB(230, 220, 200));
    } else {
        drawText(dc, 48, 296, L"Двухфазная доля не вычислена.",
            RGB(230, 220, 200));
    }

    drawText(dc, 48, 530,
        L"Управление: вверх/вниз — температура, влево/вправо — плотность, Home — сброс.",
        RGB(165, 180, 205), 32);
    drawText(dc, 48, 580,
        L"Справочник: Page Up / Page Down. Расчёт: только равновесие жидкости и пара.",
        RGB(130, 145, 170), 32);

    drawRegimeCatalog(dc);
}

void Application::drawRegimeCatalog(HDC dc) const {
    const auto& regimes = WaterRegimeCatalog::all();
    const int x = 650;
    const int width = 560;
    const WaterRegimeInfo& regime = regimes[selectedRegimeIndex_];

    drawText(dc, x, 34, L"Девять режимов вещества", RGB(240, 245, 255), 42);
    drawWrappedText(dc, x, 78, width, 42,
        L"Page Up / Page Down листают карточки. Это описание, не переключатель фаз.",
        RGB(165, 180, 205));

    std::wstring counter = L"Карточка " +
        std::to_wstring(selectedRegimeIndex_ + 1) + L" из " +
        std::to_wstring(regimes.size());
    drawText(dc, x, 132, counter, RGB(165, 180, 205), 28);
    drawWrappedText(dc, x, 178, width, 48, regime.name,
        RGB(110, 205, 240));

    drawText(dc, x, 238, L"Как ведёт себя вещество", RGB(240, 245, 255), 28);
    drawWrappedText(dc, x, 274, width, 92, regime.behavior,
        RGB(220, 230, 245));

    drawText(dc, x, 382, L"При каких условиях", RGB(240, 245, 255), 28);
    drawWrappedText(dc, x, 418, width, 100, regime.conditions,
        RGB(190, 200, 220));

    drawText(dc, x, 534, L"Что умеет текущая модель", RGB(240, 245, 255), 28);
    drawWrappedText(dc, x, 570, width, 92, regime.modelNote,
        RGB(255, 190, 145));
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
    case WM_KEYDOWN:
        if (!app) {
            break;
        }
        if (wParam == VK_ESCAPE) {
            DestroyWindow(window);
            return 0;
        }
        if (wParam == VK_PRIOR) {
            const std::size_t count = WaterRegimeCatalog::all().size();
            app->selectedRegimeIndex_ =
                (app->selectedRegimeIndex_ + count - 1) % count;
        } else if (wParam == VK_NEXT) {
            const std::size_t count = WaterRegimeCatalog::all().size();
            app->selectedRegimeIndex_ =
                (app->selectedRegimeIndex_ + 1) % count;
        } else if (wParam == VK_UP) {
            app->temperatureK_ = std::min(
                app->temperatureK_ + 1.0,
                WaterPhaseModel::CRITICAL_POINT_K);
        } else if (wParam == VK_DOWN) {
            app->temperatureK_ = std::max(
                app->temperatureK_ - 1.0,
                WaterPhaseModel::TRIPLE_POINT_K);
        } else if (wParam == VK_RIGHT) {
            app->densityKgM3_ = std::min(app->densityKgM3_ * 1.1, 1500.0);
        } else if (wParam == VK_LEFT) {
            app->densityKgM3_ = std::max(app->densityKgM3_ / 1.1, 0.001);
        } else if (wParam == VK_HOME) {
            app->temperatureK_ = 373.15;
            app->densityKgM3_ = 50.0;
        } else {
            return 0;
        }
        app->updateDashboard();
        return 0;
    case WM_DESTROY:
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
    windowClass.lpszClassName = L"WaterThermodynamicsWindow";
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(
        GetStockObject(BLACK_BRUSH));
    RegisterClassW(&windowClass);

    RECT bounds{0, 0, WIDTH, HEIGHT};
    AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);

    window_ = CreateWindowW(
        windowClass.lpszClassName,
        L"Симуляция термодинамики воды",
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
    ShowWindow(window_, SW_SHOW);
}
