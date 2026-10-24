#pragma once

#include "../core/ParticleSystem.h"
#include "EwaldElectrostatics.h"

namespace physics {

// Гибкая нереактивная модель типа SPC/Fw: молекулы не разрываются.
// Связи и углы задают энергию деформации; заряды и O-O потенциал
// создают притяжение, отталкивание и ориентацию соседних молекул.
class WaterForceField {
public:
    ForceEvaluation evaluate(ParticleSystem& system) const;
    static constexpr double bondLengthA = 1.012;
    static constexpr double angleRad = 113.24 * units::pi / 180.0;
private:
    EwaldElectrostatics electrostatics_;
    void intramolecular(ParticleSystem& system, double& energy) const;
    void intermolecular(ParticleSystem& system, double& energy) const;
};

} // namespace physics
