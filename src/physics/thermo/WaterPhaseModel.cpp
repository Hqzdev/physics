#include "WaterPhaseModel.h"

#include <cmath>

namespace {
constexpr double CRITICAL_TEMPERATURE_K = WaterPhaseModel::CRITICAL_POINT_K;
constexpr double CRITICAL_PRESSURE = WaterPhaseModel::CRITICAL_PRESSURE_PA;
constexpr double CRITICAL_DENSITY = WaterPhaseModel::CRITICAL_DENSITY_KG_M3;

double reducedTemperatureDistance(double temperatureK) {
    return 1.0 - temperatureK / CRITICAL_TEMPERATURE_K;
}
}

double WaterPhaseModel::saturationPressurePa(double temperatureK) {
    const double theta = reducedTemperatureDistance(temperatureK);

    // Корреляция IAPWS для давления насыщения. theta стремится к нулю при Tc.
    const double exponent = (CRITICAL_TEMPERATURE_K / temperatureK) * (
        -7.85951783 * theta
        + 1.84408259 * std::pow(theta, 1.5)
        - 11.7866497 * std::pow(theta, 3.0)
        + 22.6807411 * std::pow(theta, 3.5)
        - 15.9618719 * std::pow(theta, 4.0)
        + 1.80122502 * std::pow(theta, 7.5));

    return CRITICAL_PRESSURE * std::exp(exponent);
}

double WaterPhaseModel::saturatedLiquidDensity(double temperatureK) {
    const double theta = reducedTemperatureDistance(temperatureK);
    const double thetaOneThird = std::cbrt(theta);
    const double thetaTwoThirds = thetaOneThird * thetaOneThird;

    // Вспомогательная корреляция IAPWS для плотности насыщенной жидкости.
    const double densityRatio = 1.0
        + 1.99274064 * thetaOneThird
        + 1.09965342 * thetaTwoThirds
        - 0.510839303 * std::pow(theta, 5.0 / 3.0)
        - 1.75493479 * std::pow(theta, 16.0 / 3.0)
        - 45.5170352 * std::pow(theta, 43.0 / 3.0)
        - 674694.45 * std::pow(theta, 110.0 / 3.0);

    return CRITICAL_DENSITY * densityRatio;
}

double WaterPhaseModel::saturatedVaporDensity(double temperatureK) {
    const double theta = reducedTemperatureDistance(temperatureK);
    const double thetaOneThird = std::cbrt(theta);

    // Вспомогательная корреляция IAPWS для плотности насыщенного пара.
    const double exponent =
        -2.03150240 * thetaOneThird
        - 2.68302940 * std::pow(theta, 2.0 / 3.0)
        - 5.38626492 * std::pow(theta, 4.0 / 3.0)
        - 17.2991605 * std::pow(theta, 3.0)
        - 44.7586581 * std::pow(theta, 37.0 / 6.0)
        - 63.9201063 * std::pow(theta, 71.0 / 6.0);

    return CRITICAL_DENSITY * std::exp(exponent);
}

bool WaterPhaseModel::saturationAt(double temperatureK, SaturationState& result) const {
    if (temperatureK < TRIPLE_POINT_K || temperatureK > CRITICAL_POINT_K) {
        return false;
    }

    if (temperatureK == CRITICAL_POINT_K) {
        result = {
            temperatureK,
            CRITICAL_PRESSURE,
            CRITICAL_DENSITY,
            CRITICAL_DENSITY
        };
        return true;
    }

    result = {
        temperatureK,
        saturationPressurePa(temperatureK),
        saturatedLiquidDensity(temperatureK),
        saturatedVaporDensity(temperatureK)
    };
    return true;
}

WaterPhaseModel::Result WaterPhaseModel::evaluate(
    double temperatureK,
    double overallDensityKgM3) const {
    Result result{};
    result.temperatureK = temperatureK;
    result.overallDensityKgM3 = overallDensityKgM3;

    if (!std::isfinite(temperatureK) || !std::isfinite(overallDensityKgM3) ||
        overallDensityKgM3 <= 0.0) {
        result.region = Region::Unsupported;
        result.message = "Enter a finite temperature and positive density.";
        return result;
    }

    SaturationState saturation{};
    if (!saturationAt(temperatureK, saturation)) {
        result.region = Region::Unsupported;
        result.message =
            "This model covers liquid-vapor equilibrium from the triple point "
            "to the critical point only.";
        return result;
    }

    result.liquidDensityKgM3 = saturation.liquidDensityKgM3;
    result.vaporDensityKgM3 = saturation.vaporDensityKgM3;

    if (temperatureK == TRIPLE_POINT_K) {
        result.region = Region::TriplePoint;
        result.pressurePa = saturation.pressurePa;
        result.pressureAvailable = true;
        result.message =
            "Ice, liquid, and vapor can coexist; phase fractions require a solid model.";
        return result;
    }

    // Две фазы могут сосуществовать, только если общая плотность находится
    // между плотностями насыщенного пара и насыщенной жидкости при этой температуре.
    const double rhoVapor = saturation.vaporDensityKgM3;
    const double rhoLiquid = saturation.liquidDensityKgM3;

    if (temperatureK == CRITICAL_POINT_K) {
        const double densityTolerance = 1e-6 * CRITICAL_DENSITY;
        if (std::abs(overallDensityKgM3 - CRITICAL_DENSITY) <= densityTolerance) {
            result.region = Region::CriticalPoint;
            result.pressurePa = saturation.pressurePa;
            result.pressureAvailable = true;
            result.message =
                "At the critical point, liquid and vapor become indistinguishable.";
        } else {
            result.region = Region::OutsideLiquidVaporCoexistence;
            result.message =
                "Temperature is critical, but density is not the critical density; "
                "pressure is outside this saturation-only model.";
        }
        return result;
    }

    const double densityTolerance = 1e-9 * rhoLiquid;
    if (std::abs(overallDensityKgM3 - rhoLiquid) <= densityTolerance) {
        result.region = Region::SaturatedLiquid;
        result.pressurePa = saturation.pressurePa;
        result.pressureAvailable = true;
        result.message = "The state is saturated liquid water.";
        return result;
    }
    if (std::abs(overallDensityKgM3 - rhoVapor) <= densityTolerance) {
        result.region = Region::SaturatedVapor;
        result.pressurePa = saturation.pressurePa;
        result.pressureAvailable = true;
        result.vaporMassFraction = 1.0;
        result.message = "The state is saturated water vapor.";
        return result;
    }

    if (overallDensityKgM3 <= rhoVapor || overallDensityKgM3 >= rhoLiquid) {
        result.region = Region::OutsideLiquidVaporCoexistence;
        result.message =
            "Bulk density is outside the liquid-vapor coexistence interval; "
            "pressure requires a single-phase equation of state.";
        return result;
    }

    // Правило рычага по удельному объёму даёт массовую долю пара.
    const double bulkSpecificVolume = 1.0 / overallDensityKgM3;
    const double liquidSpecificVolume = 1.0 / rhoLiquid;
    const double vaporSpecificVolume = 1.0 / rhoVapor;
    result.vaporMassFraction =
        (bulkSpecificVolume - liquidSpecificVolume) /
        (vaporSpecificVolume - liquidSpecificVolume);
    result.region = Region::LiquidVaporMixture;
    result.pressurePa = saturation.pressurePa;
    result.pressureAvailable = true;
    result.message = "Liquid and vapor coexist at the saturation pressure.";
    return result;
}
