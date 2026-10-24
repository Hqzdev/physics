#include "LangevinIntegrator.h"

#include <cmath>

namespace physics {

void LangevinIntegrator::kick(ParticleSystem& system, double dtFs) const {
    for (auto& p : system.particles) p.velocityAperFs += p.force * (units::acceleration * dtFs / p.massAmu);
}

void LangevinIntegrator::drift(ParticleSystem& system, double dtFs) const {
    for (auto& p : system.particles) {
        const Vec3 delta = p.velocityAperFs * dtFs;
        p.positionA += delta;
        p.transportA += delta;
    }
}

void LangevinIntegrator::thermalize(ParticleSystem& system, double dtFs, double temperatureK) {
    const double before = system.kineticEnergy();
    // Флуктуации и торможение связаны одной температурой — это уравнение
    // Ланжевена, моделирующее контакт с тепловым окружением.
    constexpr double relaxationFs = 100.0;
    const double decay = std::exp(-dtFs / relaxationFs);
    for (auto& p : system.particles) {
        const double sigma = std::sqrt((1.0 - decay * decay) *
            units::boltzmann * temperatureK * units::acceleration / p.massAmu);
        p.velocityAperFs = p.velocityAperFs * decay +
            Vec3{normal_(random_), normal_(random_), normal_(random_)} * sigma;
    }
    system.removeDrift();
    exchangedHeat_ += system.kineticEnergy() - before;
}

ForceEvaluation LangevinIntegrator::step(ParticleSystem& system, double dtFs,
    double bathTemperatureK, bool bathEnabled, const ForceCalculator& calculate) {
    kick(system, dtFs * 0.5);
    drift(system, dtFs * 0.5);
    if (bathEnabled) thermalize(system, dtFs, bathTemperatureK);
    drift(system, dtFs * 0.5);
    auto evaluation = calculate(system);
    kick(system, dtFs * 0.5);
    return evaluation;
}

} // namespace physics
