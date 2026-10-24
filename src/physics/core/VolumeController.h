#pragma once

#include "LangevinIntegrator.h"
#include <random>

namespace physics {

// Баростат Монте-Карло: пробуем изменить объём и принимаем изменение
// с вероятностью, следующей из энергии, работы P*dV и энтропии переноса.
// В отличие от присваивания давления, это меняет конфигурацию системы.
class VolumeController {
public:
    bool attempt(ParticleSystem& system, ForceEvaluation& current, double temperatureK,
                 double targetPressureMPa, const ForceCalculator& calculate);
    static double pressureMPa(const ParticleSystem& system, const ForceCalculator& calculate);
private:
    std::mt19937 random_{20261025};
    std::uniform_real_distribution<double> uniform_{0.0, 1.0};
};

} // namespace physics
