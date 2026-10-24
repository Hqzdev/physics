#pragma once

#include "ParticleSystem.h"
#include <functional>
#include <random>

namespace physics {

using ForceCalculator = std::function<ForceEvaluation(ParticleSystem&)>;

// BAOAB: половина импульса, половина движения, обмен с тепловой баней,
// половина движения, новые силы, половина импульса.
// При выключенной бане получаем обычный velocity Verlet.
class LangevinIntegrator {
public:
    explicit LangevinIntegrator(unsigned int seed = 20261024) : random_(seed) {}
    ForceEvaluation step(ParticleSystem& system, double dtFs, double bathTemperatureK,
                         bool bathEnabled, const ForceCalculator& calculate);
    double exchangedHeat() const { return exchangedHeat_; }
private:
    std::mt19937 random_;
    std::normal_distribution<double> normal_{0.0, 1.0};
    double exchangedHeat_ = 0.0;
    void kick(ParticleSystem& system, double dtFs) const;
    void drift(ParticleSystem& system, double dtFs) const;
    void thermalize(ParticleSystem& system, double dtFs, double temperatureK);
};

} // namespace physics
