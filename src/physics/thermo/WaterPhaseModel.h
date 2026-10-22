#pragma once

#include <string>

// Модель равновесия обычной жидкой воды и водяного пара.
// Поддерживается область сосуществования от тройной точки до критической.
// Лёд высокого давления, сверхионная вода, плазма и состояния ниже тройной
// точки требуют других моделей и здесь отмечаются как неподдерживаемые.
class WaterPhaseModel {
public:
enum class Region {
    // Жидкость и пар одновременно существуют в равновесии
    // при температуре и давлении на линии насыщения.
    LiquidVaporMixture,

    // Насыщенная жидкость: жидкость на границе кипения,
    // готовая перейти в пар. Доля пара равна 0.
    SaturatedLiquid,

    // Насыщенный пар: пар на границе конденсации,
    // готовый перейти в жидкость. Доля пара равна 1.
    SaturatedVapor,

// Вещество не разделено на жидкость и пар:
// оно находится в каком-то одном состоянии.
OutsideLiquidVaporCoexistence,

// Условия, при которых лёд, жидкая вода и пар
// могут существовать одновременно.
TriplePoint,

// Точка, после которой жидкость и пар становятся
// неразличимы друг от друга.
CriticalPoint,

// Это состояние программа пока не рассчитывает.
Unsupported
};
    struct SaturationState {
        double temperatureK;
        double pressurePa;
        double liquidDensityKgM3;
        double vaporDensityKgM3;
    };

    struct Result {
        Region region;
        double temperatureK;
        double pressurePa;
        bool pressureAvailable;
        double overallDensityKgM3;
        double liquidDensityKgM3;
        double vaporDensityKgM3;
        double vaporMassFraction;
        std::string message;
    };

    // Вспомогательные уравнения IAPWS для насыщения обычной воды.
    // Возвращает false за пределами диапазона от тройной до критической точки.
    bool saturationAt(double temperatureK, SaturationState& result) const;

    // Проверяет, соответствует ли общая плотность смеси жидкость–пар
    // при давлении насыщения для заданной температуры.
    Result evaluate(double temperatureK, double overallDensityKgM3) const;

    static constexpr double TRIPLE_POINT_K = 273.16;
    static constexpr double CRITICAL_POINT_K = 647.096;
    static constexpr double CRITICAL_PRESSURE_PA = 22.064e6;
    static constexpr double CRITICAL_DENSITY_KG_M3 = 322.0;

private:
    static double saturationPressurePa(double temperatureK);
    static double saturatedLiquidDensity(double temperatureK);
    static double saturatedVaporDensity(double temperatureK);
};
