#include "PlasmaForceField.h"

#include <stdexcept>

namespace physics {

ForceEvaluation PlasmaForceField::evaluate(ParticleSystem& system) const {
    double electronCount = 0.0;
    for (auto& p : system.particles) { p.force = {}; electronCount += p.chargeE; }
    const double volume = system.box.volumeA3();
    if (volume != cachedVolume_ || electronTemperatureK_ != cachedTemperature_ || electronCount != cachedCount_) {
        cachedState_ = electronGas_.evaluate(electronCount, volume, electronTemperatureK_);
        cachedVolume_ = volume;
        cachedTemperature_ = electronTemperatureK_;
        cachedCount_ = electronCount;
    }
    ForceEvaluation result;
    result.electronFreeEnergy = cachedState_.freeEnergyJ * units::jouleToKJMol;
    result.electronPressureMPa = cachedState_.pressurePa * 1.0e-6;
    result.electronTemperatureRatio = cachedState_.temperatureRatio;
    result.screeningLengthA = cachedState_.screeningLengthA;
    const double kappa = 1.0/result.screeningLengthA;
    const double rc = 0.49*system.box.sideA();
    const double uc = std::exp(-kappa*rc)/rc;
    const double fc = uc * (kappa + 1.0/rc);

    for (std::size_t i = 0; i < system.particles.size(); ++i) {
        for (std::size_t j = i + 1; j < system.particles.size(); ++j) {
            auto& a = system.particles[i];
            auto& b = system.particles[j];
            const Vec3 delta = system.box.displacement(a.positionA - b.positionA);
            const double r = norm(delta);
            if (r >= rc) continue;
            if (r < 1.0e-8) throw std::runtime_error("Overlapping plasma ions");
            const double coefficient = units::coulomb*a.chargeE*b.chargeE;
            const double u = std::exp(-kappa*r)/r;
            const double magnitude = coefficient * (u*(kappa + 1.0/r) - fc);
            result.potential += coefficient * (u - uc + (r - rc)*fc);
            const Vec3 force = delta * (magnitude/r);
            a.force += force;
            b.force -= force;
        }
    }
    return result;
}

} // namespace physics
