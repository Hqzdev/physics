#pragma once

#include "../core/ParticleSystem.h"
#include "ElectronGas.h"

namespace physics {

// Заранее полностью ионизированная смесь H+ и O8+ в электронном фоне.
// Эффективное взаимодействие ионов — экранированный потенциал Юкавы.
// Эта модель не описывает ионизацию/рекомбинацию или переход из H2O в плазму.
class PlasmaForceField {
public:
    explicit PlasmaForceField(double electronTemperatureK) : electronTemperatureK_(electronTemperatureK) {}
    void setElectronTemperature(double temperatureK) { electronTemperatureK_ = temperatureK; }
    ForceEvaluation evaluate(ParticleSystem& system) const;
private:
    double electronTemperatureK_;
    ElectronGas electronGas_;
    mutable double cachedVolume_ = -1.0, cachedTemperature_ = -1.0;
    mutable double cachedCount_ = -1.0;
    mutable ElectronState cachedState_;
};

} // namespace physics
