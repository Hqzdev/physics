#pragma once

#include "analysis/StructureAnalyzer.h"
#include "core/LangevinIntegrator.h"
#include "core/VolumeController.h"
#include "models/InitialConditions.h"
#include "models/PlasmaForceField.h"
#include "models/WaterForceField.h"
#include <variant>

namespace physics {

struct SimulationReport {
    double timeFs = 0.0, timeStepFs = 0.0;
    double temperatureK = 0.0, meanTemperatureK = 0.0;
    double pressureMPa = 0.0, meanPressureMPa = 0.0;
    double densityKgM3 = 0.0;
    double kineticEnergy = 0.0, potentialEnergy = 0.0;
    ForceEvaluation field;
    StructureObservation observation;
};

// Владеет системой и собирает физику композицией. UI видит только const-данные.
// Модель выбирается при создании эксперимента. Метки состояния и цвета
// никогда не участвуют в расчёте сил или в интеграторе.
class SimulationEngine {
public:
    SimulationEngine();
    void reset(Experiment experiment);
    void advance(unsigned int steps);
    void setTargetTemperature(double temperatureK);
    void setTargetPressure(double pressureMPa);
    void setThermostatEnabled(bool enabled);
    void setBarostatEnabled(bool enabled);
    void changeVolume(double ratio);

    const ParticleSystem& system() const { return system_; }
    const SimulationReport& report() const { return report_; }
    const ExperimentSettings& settings() const { return settings_; }
    Experiment experiment() const { return experiment_; }
    bool thermostatEnabled() const { return thermostatEnabled_; }
    bool barostatEnabled() const { return barostatEnabled_; }
    double exchangedHeat() const { return integrator_.exchangedHeat(); }
private:
    using Model = std::variant<WaterForceField, PlasmaForceField>;
    ParticleSystem system_;
    Experiment experiment_ = Experiment::Liquid;
    ExperimentSettings settings_;
    Model model_;
    LangevinIntegrator integrator_;
    VolumeController volumeController_;
    StructureAnalyzer analyzer_;
    ForceEvaluation field_;
    SimulationReport report_;
    bool thermostatEnabled_ = true, barostatEnabled_ = false;
    unsigned int stepCount_ = 0;
    ForceEvaluation calculate(ParticleSystem& system);
    double stableTimeStep() const;
    void measure(double elapsedFs);
};

} // namespace physics
