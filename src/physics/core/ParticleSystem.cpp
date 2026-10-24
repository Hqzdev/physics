#include "ParticleSystem.h"

#include <stdexcept>

namespace physics {

PeriodicBox::PeriodicBox(double sideA) : sideA_(sideA) {
    if (!std::isfinite(sideA) || sideA <= 0.0) {
        throw std::invalid_argument("Periodic box must have a positive finite side");
    }
}

Vec3 PeriodicBox::displacement(Vec3 d) const {
    d.x -= sideA_ * std::round(d.x / sideA_);
    d.y -= sideA_ * std::round(d.y / sideA_);
    d.z -= sideA_ * std::round(d.z / sideA_);
    return d;
}

Vec3 PeriodicBox::wrap(Vec3 p) const {
    p.x -= sideA_ * std::floor(p.x / sideA_);
    p.y -= sideA_ * std::floor(p.y / sideA_);
    p.z -= sideA_ * std::floor(p.z / sideA_);
    return p;
}

double ParticleSystem::massAmu() const {
    double mass = 0.0;
    for (const auto& p : particles) mass += p.massAmu;
    return mass;
}

double ParticleSystem::kineticEnergy() const {
    double energy = 0.0;
    for (const auto& p : particles) energy += units::kinetic * p.massAmu * dot(p.velocityAperFs, p.velocityAperFs);
    return energy;
}

double ParticleSystem::translationalKineticEnergy() const {
    if (molecules.empty()) return kineticEnergy();
    double energy = 0.0;
    for (const auto& indices : molecules) {
        Vec3 momentum{};
        double mass = 0.0;
        for (const auto i : indices) {
            momentum += particles[i].velocityAperFs * particles[i].massAmu;
            mass += particles[i].massAmu;
        }
        energy += units::kinetic * dot(momentum, momentum) / mass;
    }
    return energy;
}

double ParticleSystem::temperatureK() const {
    const double dof = 3.0 * static_cast<double>(particles.size()) - 3.0;
    return dof > 0.0 ? 2.0 * kineticEnergy() / (dof * units::boltzmann) : 0.0;
}

void ParticleSystem::removeDrift() {
    Vec3 momentum{};
    for (const auto& p : particles) momentum += p.velocityAperFs * p.massAmu;
    const Vec3 drift = momentum * (1.0 / massAmu());
    for (auto& p : particles) p.velocityAperFs -= drift;
}

void ParticleSystem::rescaleVolume(double volumeRatio) {
    if (!std::isfinite(volumeRatio) || volumeRatio <= 0.0) throw std::invalid_argument("Invalid volume ratio");
    const double scale = std::cbrt(volumeRatio);
    if (molecules.empty()) {
        for (auto& p : particles) p.positionA = p.positionA * scale;
    } else {
        // Сжимаем расположение молекул, сохраняя длины O-H и угол H-O-H.
        // Масштабирование всех атомов растягивало бы химические связи.
        for (const auto& indices : molecules) {
            Vec3 center{};
            double mass = 0.0;
            for (const auto i : indices) {
                center += particles[i].positionA * particles[i].massAmu;
                mass += particles[i].massAmu;
            }
            center = center * (1.0 / mass);
            const Vec3 shift = center * (scale - 1.0);
            for (const auto i : indices) particles[i].positionA += shift;
        }
    }
    box = PeriodicBox(box.sideA() * scale);
}

bool ParticleSystem::finite() const {
    for (const auto& p : particles) {
        const double values[] = {p.positionA.x, p.positionA.y, p.positionA.z,
            p.velocityAperFs.x, p.velocityAperFs.y, p.velocityAperFs.z,
            p.force.x, p.force.y, p.force.z};
        for (const double value : values) if (!std::isfinite(value)) return false;
    }
    return true;
}

} // namespace physics
