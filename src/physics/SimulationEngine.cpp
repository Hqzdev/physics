#include "SimulationEngine.h"

#include <algorithm>
#include <stdexcept>

namespace physics {

SimulationEngine::SimulationEngine() { reset(Experiment::Liquid); }

void SimulationEngine::reset(Experiment experiment) {
    experiment_ = experiment;
    settings_ = InitialConditions::settings(experiment);
    system_ = InitialConditions::create(experiment);
    if (settings_.model == ModelKind::MolecularWater) model_.emplace<WaterForceField>();
    else model_.emplace<PlasmaForceField>(settings_.temperatureK);
    integrator_ = LangevinIntegrator{};
    volumeController_ = VolumeController{};
    analyzer_ = StructureAnalyzer{};
    report_ = {};
    stepCount_ = 0;
    thermostatEnabled_ = true;
    barostatEnabled_ = false;
    field_ = calculate(system_);
    measure(0.0);
}

ForceEvaluation SimulationEngine::calculate(ParticleSystem& system) {
    return std::visit([&](const auto& model) { return model.evaluate(system); }, model_);
}

double SimulationEngine::stableTimeStep() const {
    double nearest = system_.box.sideA(), maxSpeed = 0.0, maxAcceleration = 0.0;
    for (std::size_t i = 0; i < system_.particles.size(); ++i) {
        const auto& p = system_.particles[i];
        maxSpeed = std::max(maxSpeed, norm(p.velocityAperFs));
        maxAcceleration = std::max(maxAcceleration, norm(p.force)*units::acceleration/p.massAmu);
        for (std::size_t j = i + 1; j < system_.particles.size(); ++j)
            nearest = std::min(nearest, norm(system_.box.displacement(p.positionA - system_.particles[j].positionA)));
    }
    double dt = settings_.model == ModelKind::MolecularWater ? .2 : .02;
    if (maxSpeed > 0.0) dt = std::min(dt, .03*nearest/maxSpeed);
    if (maxAcceleration > 0.0) dt = std::min(dt, std::sqrt(.03*nearest/maxAcceleration));
    if (!std::isfinite(dt) || dt < 1.0e-7) throw std::runtime_error("Time step collapsed; reset the experiment");
    return dt;
}

void SimulationEngine::advance(unsigned int steps) {
    const auto force = [this](ParticleSystem& system) { return calculate(system); };
    double elapsed = 0.0;
    for (unsigned int i = 0; i < steps; ++i) {
        const double dt = stableTimeStep();
        const auto backup = system_;
        try {
            field_ = integrator_.step(system_, dt, settings_.temperatureK, thermostatEnabled_, force);
            if (!system_.finite() || !std::isfinite(field_.potential)) throw std::runtime_error("Non-finite integration result");
        } catch (...) {
            system_ = backup;
            field_ = calculate(system_);
            throw;
        }
        report_.timeFs += dt;
        report_.timeStepFs = dt;
        elapsed += dt;
        if (barostatEnabled_ && ++stepCount_%32 == 0)
            volumeController_.attempt(system_, field_, settings_.temperatureK, settings_.pressureMPa, force);
    }
    measure(elapsed);
}

void SimulationEngine::measure(double elapsedFs) {
    report_.field = field_;
    report_.temperatureK = system_.temperatureK();
    report_.pressureMPa = VolumeController::pressureMPa(system_, [this](ParticleSystem& s) { return calculate(s); });
    report_.densityKgM3 = system_.massAmu()*units::amuPerA3ToKgM3/system_.box.volumeA3();
    report_.kineticEnergy = system_.kineticEnergy();
    report_.potentialEnergy = field_.potential;
    const double blend = report_.timeFs == 0.0 ? 1.0 : 1.0 - std::exp(-elapsedFs/100.0);
    report_.meanTemperatureK += blend*(report_.temperatureK - report_.meanTemperatureK);
    report_.meanPressureMPa += blend*(report_.pressureMPa - report_.meanPressureMPa);
    report_.observation = analyzer_.analyze(system_, report_.timeFs,
        report_.meanTemperatureK, report_.meanPressureMPa, field_.electronTemperatureRatio, settings_.model);
}

void SimulationEngine::setTargetTemperature(double temperatureK) {
    if (!std::isfinite(temperatureK)) throw std::invalid_argument("Invalid temperature");
    settings_.temperatureK = settings_.model == ModelKind::MolecularWater ?
        std::clamp(temperatureK, 1.0, 1000.0) : std::clamp(temperatureK, 1.0e5, 1.0e8);
    if (auto* plasma = std::get_if<PlasmaForceField>(&model_)) plasma->setElectronTemperature(settings_.temperatureK);
    field_ = calculate(system_);
    measure(0.0);
}

void SimulationEngine::setTargetPressure(double pressureMPa) {
    if (!std::isfinite(pressureMPa)) throw std::invalid_argument("Invalid pressure");
    settings_.pressureMPa = std::clamp(pressureMPa, 1.0e-5, 1.0e12);
}

void SimulationEngine::setThermostatEnabled(bool enabled) {
    thermostatEnabled_ = enabled;
    if (!enabled) barostatEnabled_ = false;
}

void SimulationEngine::setBarostatEnabled(bool enabled) {
    barostatEnabled_ = enabled;
    if (enabled) thermostatEnabled_ = true;
}

void SimulationEngine::changeVolume(double ratio) {
    if (!std::isfinite(ratio) || ratio <= 0.0) throw std::invalid_argument("Invalid volume ratio");
    const double density = system_.massAmu()*units::amuPerA3ToKgM3/(system_.box.volumeA3()*ratio);
    const bool molecular = settings_.model == ModelKind::MolecularWater;
    if (density < (molecular ? .05 : 10.0) || density > (molecular ? 2000.0 : 1.0e7)) return;
    system_.rescaleVolume(ratio);
    field_ = calculate(system_);
    measure(0.0);
}

} // namespace physics
