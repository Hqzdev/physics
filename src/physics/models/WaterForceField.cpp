#include "WaterForceField.h"

#include <algorithm>
#include <stdexcept>

namespace physics {
namespace {
constexpr double bondStiffness = 4431.13;
constexpr double angleStiffness = 317.57;
constexpr double sigmaA = 3.16549;
constexpr double epsilon = 0.6502;
constexpr double cutoffA = 9.0;
}

ForceEvaluation WaterForceField::evaluate(ParticleSystem& system) const {
    for (auto& p : system.particles) p.force = {};
    ForceEvaluation result;
    intramolecular(system, result.potential);
    intermolecular(system, result.potential);
    result.potential += electrostatics_.accumulate(system);
    return result;
}

void WaterForceField::intramolecular(ParticleSystem& system, double& energy) const {
    for (const auto& indices : system.molecules) {
        auto& o = system.particles[indices[0]];
        auto& h1 = system.particles[indices[1]];
        auto& h2 = system.particles[indices[2]];
        const Vec3 a = system.box.displacement(h1.positionA - o.positionA);
        const Vec3 b = system.box.displacement(h2.positionA - o.positionA);
        const double ra = norm(a), rb = norm(b);
        if (ra < 1.0e-8 || rb < 1.0e-8) throw std::runtime_error("Collapsed O-H bond");
        const Vec3 fa = a * (-bondStiffness * (ra - bondLengthA) / ra);
        const Vec3 fb = b * (-bondStiffness * (rb - bondLengthA) / rb);
        h1.force += fa;
        h2.force += fb;
        o.force -= fa + fb;
        energy += 0.5 * bondStiffness * ((ra - bondLengthA)*(ra - bondLengthA) +
            (rb - bondLengthA)*(rb - bondLengthA));

        const Vec3 ua = a * (1.0 / ra), ub = b * (1.0 / rb);
        const double cosine = std::clamp(dot(ua, ub), -1.0, 1.0);
        const double difference = std::acos(cosine) - angleRad;
        const double sine = std::sqrt(std::max(1.0e-12, 1.0 - cosine*cosine));
        const Vec3 angleA = (ub - ua * cosine) * (angleStiffness * difference / (ra * sine));
        const Vec3 angleB = (ua - ub * cosine) * (angleStiffness * difference / (rb * sine));
        h1.force += angleA;
        h2.force += angleB;
        o.force -= angleA + angleB;
        energy += 0.5 * angleStiffness * difference * difference;
    }
}

void WaterForceField::intermolecular(ParticleSystem& system, double& energy) const {
    const double rc = std::min(cutoffA, 0.49 * system.box.sideA());
    const double sc = std::pow(sigmaA / rc, 6);
    const double ljAtCutoff = 4.0 * epsilon * (sc*sc - sc);
    const double ljForceAtCutoff = 24.0 * epsilon / rc * sc * (2.0*sc - 1.0);

    for (std::size_t i = 0; i < system.molecules.size(); ++i) {
        for (std::size_t j = i + 1; j < system.molecules.size(); ++j) {
            auto& a = system.particles[system.molecules[i][0]];
            auto& b = system.particles[system.molecules[j][0]];
            const Vec3 delta = system.box.displacement(a.positionA - b.positionA);
            const double r = norm(delta);
            if (r >= rc) continue;
            if (r < 1.0e-6) throw std::runtime_error("Overlapping oxygen atoms");
            const double s = std::pow(sigmaA / r, 6);
            const double magnitude = 24.0 * epsilon / r * s * (2.0*s - 1.0) - ljForceAtCutoff;
            energy += 4.0 * epsilon * (s*s - s) - ljAtCutoff + (r - rc)*ljForceAtCutoff;
            const Vec3 force = delta * (magnitude / r);
            a.force += force;
            b.force -= force;
        }
    }
}

} // namespace physics
