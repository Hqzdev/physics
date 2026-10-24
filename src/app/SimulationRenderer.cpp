#include "SimulationRenderer.h"
#include "StateAppearance.h"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <vector>

namespace {
constexpr COLORREF foreground = RGB(227, 235, 249);
constexpr COLORREF secondary = RGB(153, 173, 199);

class SelectedFont {
public:
    explicit SelectedFont(HDC dc, int height = 18) : dc_(dc) {
        font_ = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        if (font_) previous_ = SelectObject(dc_, font_);
    }
    ~SelectedFont() { if (font_) { SelectObject(dc_, previous_); DeleteObject(font_); } }
    SelectedFont(const SelectedFont&) = delete;
    SelectedFont& operator=(const SelectedFont&) = delete;
private:
    HDC dc_;
    HFONT font_ = nullptr;
    HGDIOBJ previous_ = nullptr;
};

std::wstring number(double value, int precision = 2) {
    std::wostringstream out;
    if (std::abs(value) >= 1.0e5 || (value != 0.0 && std::abs(value) < .001)) out << std::scientific;
    else out << std::fixed;
    out << std::setprecision(precision) << value;
    return out.str();
}

void text(HDC dc, int x, int y, int width, int height, const std::wstring& value, COLORREF color = foreground) {
    SetTextColor(dc, color);
    RECT box{x, y, x + width, y + height};
    DrawTextW(dc, value.c_str(), -1, &box, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
}

void line(HDC dc, POINT a, POINT b, COLORREF color, int thickness = 1) {
    HPEN pen = CreatePen(PS_SOLID, thickness, color);
    HGDIOBJ previous = SelectObject(dc, pen);
    MoveToEx(dc, a.x, a.y, nullptr);
    LineTo(dc, b.x, b.y);
    SelectObject(dc, previous);
    DeleteObject(pen);
}

void circle(HDC dc, POINT center, int radius, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    const auto oldBrush = SelectObject(dc, brush);
    const auto oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, center.x - radius, center.y - radius, center.x + radius + 1, center.y + radius + 1);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

std::wstring structureName(physics::ObservedStructure structure) {
    using physics::ObservedStructure;
    switch (structure) {
    case ObservedStructure::IceLike: return L"Кристаллическая малоподвижная структура, похожая на лёд";
    case ObservedStructure::VaporLike: return L"В основном разреженные молекулы: газоподобная структура";
    case ObservedStructure::Heterogeneous: return L"Плотные области и разреженные молекулы: кандидат на сосуществование";
    case ObservedStructure::HotFluidCandidate: return L"Горячий флюид; T/P выше справочной критической точки H2O";
    case ObservedStructure::Plasma: return L"Ионы H+ / O8+ в электронном фоне";
    case ObservedStructure::DegeneratePlasma: return L"Ионы H+ / O8+; электронный фон вырожден (T_e < T_F)";
    case ObservedStructure::MolecularFluid: return L"В основном плотная молекулярная структура";
    }
    return L"";
}
}

void SimulationRenderer::drawParticles(HDC dc, const physics::SimulationEngine& engine, int left, int top, int side) const {
    const auto& system = engine.system();
    const double length = system.box.sideA();
    auto project = [&](physics::Vec3 p) {
        return POINT{left + static_cast<LONG>((p.x + .28*p.z)/length*side),
            top + static_cast<LONG>((p.y + .18*p.z)/length*side)};
    };
    for (int i = 0; i < 8; ++i) for (int axis = 0; axis < 3; ++axis) {
        const int j = i ^ (1 << axis);
        if (j <= i) continue;
        const physics::Vec3 a{(i&1) ? length : 0.0, (i&2) ? length : 0.0, (i&4) ? length : 0.0};
        const physics::Vec3 b{(j&1) ? length : 0.0, (j&2) ? length : 0.0, (j&4) ? length : 0.0};
        line(dc, project(a), project(b), RGB(61, 80, 105));
    }
    struct DrawParticle { physics::Vec3 position; std::size_t index; };
    std::vector<DrawParticle> particles;
    if (system.molecules.empty()) {
        for (std::size_t i = 0; i < system.particles.size(); ++i)
            particles.push_back({system.box.wrap(system.particles[i].positionA), i});
    } else {
        for (const auto& molecule : system.molecules) {
            const auto oxygen = system.box.wrap(system.particles[molecule[0]].positionA);
            particles.push_back({oxygen, molecule[0]});
            for (std::size_t h = 1; h < 3; ++h) {
                const auto hydrogen = oxygen + system.box.displacement(
                    system.particles[molecule[h]].positionA - system.particles[molecule[0]].positionA);
                line(dc, project(oxygen), project(hydrogen), RGB(104, 132, 163), 2);
                particles.push_back({hydrogen, molecule[h]});
            }
        }
    }
    std::sort(particles.begin(), particles.end(), [](const auto& a, const auto& b) { return a.position.z < b.position.z; });
    for (const auto& item : particles) {
        const bool oxygen = system.particles[item.index].element == physics::Element::Oxygen;
        auto color = StateAppearance::forState(engine.report().observation.particleStates[item.index]);
        if (!oxygen) {
            color.red = static_cast<std::uint8_t>((color.red + 255)/2);
            color.green = static_cast<std::uint8_t>((color.green + 255)/2);
            color.blue = static_cast<std::uint8_t>((color.blue + 255)/2);
        }
        circle(dc, project(item.position), oxygen ? 7 : 4, RGB(color.red, color.green, color.blue));
    }
}

void SimulationRenderer::drawMeasurements(HDC dc, const physics::SimulationEngine& engine, int left, int width) const {
    const auto& report = engine.report();
    const auto& config = engine.settings();
    const bool molecular = config.model == physics::ModelKind::MolecularWater;
    text(dc, left, 73, width, 30, molecular ? L"Модель: гибкая молекула H2O" : L"Модель: ионы + идеальный газ электронов");
    text(dc, left, 109, width, 62, structureName(report.observation.structure), RGB(119, 228, 190));
    text(dc, left, 180, width, 26, std::wstring(molecular ? L"T бани: " : L"T электронов / целевая T ионов: ") + number(config.temperatureK) + L" K");
    text(dc, left, 212, width, 26, L"T частиц: " + number(report.temperatureK) + L" K");
    text(dc, left, 244, width, 26, L"P мгновенное: " + number(report.pressureMPa) + L" МПа");
    text(dc, left, 276, width, 26, L"P усреднённое: " + number(report.meanPressureMPa) + L" МПа");
    text(dc, left, 308, width, 26, L"Плотность: " + number(report.densityKgM3) + L" кг/м³");
    text(dc, left, 340, width, 26, L"Время: " + number(report.timeFs/1000.0, 4) + L" пс; шаг: " + number(report.timeStepFs, 4) + L" фс");
    text(dc, left, 372, width, 26, L"K + U частиц: " + number(report.kineticEnergy + report.potentialEnergy) + L" кДж/моль");
    text(dc, left, 404, width, 48, engine.barostatEnabled() ?
        L"Баня + баростат: целевое P = " + number(config.pressureMPa) + L" МПа" :
        engine.thermostatEnabled() ? L"Тепловая баня включена; объём фиксирован" : L"Баня выключена; объём фиксирован");
    if (molecular) {
        const auto& observation = report.observation;
        text(dc, left, 460, width, 26, L"Тетраэдрический порядок: " + number(observation.tetrahedralOrder, 3));
        text(dc, left, 492, width, 26, L"Соседей O: " + number(observation.oxygenNeighbors));
        text(dc, left, 524, width, 26, L"Льдоподобных: " + number(100.0*observation.crystallineFraction, 1) + L"%; газоподобных: " + number(100.0*observation.vaporFraction, 1) + L"%");
        text(dc, left, 560, width, 85,
            L"Цвет — оценка локальной структуры. Электростатика: Эвальд. Маленькая ячейка ограничивает точность фаз. "
            L"Суперионная вода и распад молекул здесь не рассчитываются.", secondary);
    } else {
        text(dc, left, 460, width, 26, L"Вырождение: T_e / T_F = " + number(report.field.electronTemperatureRatio, 3));
        text(dc, left, 492, width, 26, L"Экранирование: " + number(report.field.screeningLengthA, 4) + L" Å");
        text(dc, left, 524, width, 26, L"Давление электронов: " + number(report.field.electronPressureMPa) + L" МПа");
        text(dc, left, 560, width, 85,
            L"Полная ионизация задана при старте. Электроны — изотермический фон, а не точки. "
            L"Рекомбинация, излучение и релятивистские эффекты не рассчитываются.", secondary);
    }
}

void SimulationRenderer::draw(HDC dc, const RECT& client, const physics::SimulationEngine& engine,
    bool paused, unsigned int stepsPerFrame, const std::wstring& error) const {
    const int width = static_cast<int>(client.right - client.left);
    const int height = static_cast<int>(client.bottom - client.top);
    HBRUSH background = CreateSolidBrush(RGB(12, 18, 29));
    FillRect(dc, &client, background);
    DeleteObject(background);
    SetBkMode(dc, TRANSPARENT);
    SelectedFont font(dc);
    text(dc, 30, 22, width - 60, 32,
        L"Физика воды  /  " + std::wstring(paused ? L"ПАУЗА" : L"РАСЧЁТ") + L"  /  шагов за кадр: " + std::to_wstring(stepsPerFrame));
    const int panelLeft = static_cast<int>(width*.61);
    const int side = std::max(100, std::min(static_cast<int>((panelLeft - 70)/1.28), height - 305));
    drawParticles(dc, engine, 30, 98, side);
    text(dc, 30, 108 + static_cast<int>(side*1.18), panelLeft - 60, 54,
        L"Крупные точки — O, маленькие — H. Связи показаны только в молекулярной модели. "
        L"Куб периодический; видны проекции трёхмерных координат.", secondary);
    drawMeasurements(dc, engine, panelLeft, width - panelLeft - 30);
    SelectedFont footerFont(dc, 16);
    using physics::LocalState;
    const std::array<LocalState, 8> states{LocalState::IceLike, LocalState::LiquidLike, LocalState::VaporLike,
        LocalState::Interface, LocalState::HotFluid, LocalState::Plasma, LocalState::DegeneratePlasma, LocalState::Unknown};
    for (std::size_t i = 0; i < states.size(); ++i) {
        const auto appearance = StateAppearance::forState(states[i]);
        const int x = 30 + static_cast<int>(i%4)*(width - 60)/4;
        const int y = height - 148 + static_cast<int>(i/4)*28;
        circle(dc, {x + 5, y + 9}, 5, RGB(appearance.red, appearance.green, appearance.blue));
        text(dc, x + 18, y, (width - 60)/4 - 20, 26, appearance.label, secondary);
    }
    text(dc, 30, height - 85, width - 60, 26,
        L"1 жидкость · 2 затравка льда · 3 пар · 4 слой + газ · 5 горячий флюид · 6 плазма · 7 плотная плазма", secondary);
    text(dc, 30, height - 56, width - 60, 48, error.empty() ?
        L"↑/↓ температура · ←/→ объём (с B: целевое давление) · B баростат · T баня · Space пауза · Home сброс · +/- скорость" : error,
        error.empty() ? secondary : RGB(255, 116, 116));
}
