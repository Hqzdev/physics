#include "VolumeController.h"

#include <cmath>

namespace physics {

bool VolumeController::attempt(ParticleSystem& system, ForceEvaluation& current,
    double temperatureK, double targetPressureMPa, const ForceCalculator& calculate) {
    ParticleSystem candidate = system;
    const double logRatio = (2.0*uniform_(random_) - 1.0)*0.02;
    candidate.rescaleVolume(std::exp(logRatio));
    const auto proposed = calculate(candidate);
    const double deltaVolume = candidate.box.volumeA3() - system.box.volumeA3();
    const double work = targetPressureMPa * deltaVolume / units::pressureToMPa;
    const double groups = static_cast<double>(system.molecules.empty() ? system.particles.size() : system.molecules.size());
    // +1 возникает из симметричного предложения в log(V), а не в V.
    const double logAcceptance = -(proposed.freeEnergy() - current.freeEnergy() + work) /
        (units::boltzmann*temperatureK) + (groups + 1.0)*logRatio;
    if (std::log(1.0 - uniform_(random_)) < logAcceptance) {
        system = std::move(candidate);
        current = proposed;
        return true;
    }
    return false;
}

double VolumeController::pressureMPa(const ParticleSystem& system, const ForceCalculator& calculate) {
    // Виртуальное малое сжатие даёт -dF/dV. Такой оценщик учитывает
    // зависимость экранирования и электронного фона от плотности.
    // Молекулы масштабируются целиком; кинетический член берётся по их центрам масс.
    constexpr double delta = 1.0e-4;
    auto compressed = system, expanded = system;
    compressed.rescaleVolume(std::exp(-delta));
    expanded.rescaleVolume(std::exp(delta));
    const double derivative = (calculate(expanded).freeEnergy() - calculate(compressed).freeEnergy()) /
        (expanded.box.volumeA3() - compressed.box.volumeA3());
    // При фиксированном общем импульсе возвращаем недостающий вклад
    // одной поступательной моды конечной системы.
    const double ideal = (2.0*system.translationalKineticEnergy()/3.0 +
        units::boltzmann*system.temperatureK()) / system.box.volumeA3();
    return (ideal - derivative) * units::pressureToMPa;
}

} // namespace physics
