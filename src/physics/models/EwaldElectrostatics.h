#pragma once

#include "../core/ParticleSystem.h"

namespace physics {

// Периодическая электростатика нейтральной кубической ячейки:
// реальное пространство + обратное пространство + поправка собственного поля.
// Граничные условия снаружи — проводящие (tin-foil).
class EwaldElectrostatics {
public:
    // Число волн и alpha*L задают численную точность, а не фазу вещества.
    explicit EwaldElectrostatics(int reciprocalRadius = 6, double alphaLength = 6.0);
    double accumulate(ParticleSystem& system) const;
private:
    int reciprocalRadius_;
    double alphaLength_;
};

} // namespace physics
