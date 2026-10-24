#include "physics/SimulationEngine.h"
#include "app/StateAppearance.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace physics;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void close(double value, double expected, double tolerance, const std::string& message) {
    require(std::isfinite(value) && std::abs(value - expected) <= tolerance, message +
        ": got " + std::to_string(value) + ", expected " + std::to_string(expected));
}

void periodicAndScaling() {
    PeriodicBox box(10.0);
    close(box.wrap({-21, 31, 0}).x, 9.0, 1.e-12, "periodic wrapping");
    close(box.displacement({9, 0, 0}).x, -1.0, 1.e-12, "minimum image");
    auto water = InitialConditions::create(Experiment::Liquid);
    const auto bond = water.particles[1].positionA - water.particles[0].positionA;
    water.rescaleVolume(1.2);
    close(norm(water.particles[1].positionA - water.particles[0].positionA), norm(bond), 1.e-12, "barostat preserves O-H geometry");
}

void gradient(ParticleSystem system, const ForceCalculator& evaluate) {
    evaluate(system);
    Vec3 total{};
    for (const auto& p : system.particles) total += p.force;
    close(norm(total), 0.0, 1.e-7, "Newton third law");
    constexpr double step = 1.e-5;
    for (std::size_t i = 0; i < system.particles.size(); ++i) {
        const auto force = system.particles[i].force;
        for (int axis = 0; axis < 3; ++axis) {
            auto plus = system, minus = system;
            auto component = [&](Vec3& v) -> double& { return axis == 0 ? v.x : axis == 1 ? v.y : v.z; };
            component(plus.particles[i].positionA) += step;
            component(minus.particles[i].positionA) -= step;
            const double derivative = -(evaluate(plus).freeEnergy() - evaluate(minus).freeEnergy())/(2*step);
            const double actual = axis == 0 ? force.x : axis == 1 ? force.y : force.z;
            close(actual, derivative, 1.e-4*(1.0 + std::abs(actual)), "force = negative energy gradient");
        }
    }
}

void waterForces() {
    auto system = InitialConditions::create(Experiment::Vapor);
    system.particles.resize(6);
    system.molecules.resize(2);
    system.box = PeriodicBox(30.0);
    const Vec3 shift = system.particles[0].positionA + Vec3{3.1, .3, .2} - system.particles[3].positionA;
    for (std::size_t i = 3; i < 6; ++i) system.particles[i].positionA += shift;
    system.particles[1].positionA.x += .02;
    WaterForceField model;
    gradient(system, [&](ParticleSystem& s) { return model.evaluate(s); });
}

void plasmaForces() {
    auto system = InitialConditions::create(Experiment::HotPlasma);
    system.particles.resize(6);
    PlasmaForceField model(2.e7);
    gradient(system, [&](ParticleSystem& s) { return model.evaluate(s); });
}

void ewaldMadelung() {
    ParticleSystem system;
    system.box = PeriodicBox(2.0);
    for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) for (int z = 0; z < 2; ++z)
        system.particles.push_back({{double(x),double(y),double(z)}, {}, {}, {}, 1.0,
            (x+y+z)%2 ? -1.0 : 1.0, Element::Hydrogen});
    EwaldElectrostatics ewald;
    const double energy = ewald.accumulate(system);
    constexpr double madelung = 1.747564594633182;
    const double expected = -4.0*madelung*units::coulomb;
    close(energy, expected, std::abs(expected)*1.e-4, "NaCl Madelung energy");
    for (auto& p : system.particles) { p.force = {}; p.positionA += Vec3{.31, .72, -.49}; }
    close(ewald.accumulate(system), energy, std::abs(energy)*1.e-12, "periodic electrostatic translation invariance");
}

void canonicalBath() {
    ParticleSystem system;
    system.box = PeriodicBox(100.0);
    for (int i = 0; i < 64; ++i) system.particles.push_back({{double(i),0,0},{},{},{},1.0,0.0,Element::Hydrogen});
    LangevinIntegrator integrator;
    const auto zero = [](ParticleSystem&) { return ForceEvaluation{}; };
    double total = 0.0;
    constexpr int burnIn = 1000, samples = 10000;
    for (int i = 0; i < burnIn + samples; ++i) {
        integrator.step(system, 1.0, 300.0, true, zero);
        if (i >= burnIn) total += system.temperatureK();
    }
    close(total/samples, 300.0, 15.0, "Langevin bath samples target temperature");
    Vec3 momentum{};
    for (const auto& p : system.particles) momentum += p.velocityAperFs*p.massAmu;
    close(norm(momentum), 0.0, 1.e-10, "thermostat preserves zero total momentum");
}

void verletEnergy() {
    auto system = InitialConditions::create(Experiment::Vapor);
    system.particles.resize(3);
    system.molecules.resize(1);
    system.box = PeriodicBox(50.0);
    system.removeDrift();
    system.particles[1].positionA.x += .03;
    WaterForceField model;
    auto calculate = [&](ParticleSystem& s) { return model.evaluate(s); };
    const double initial = system.kineticEnergy() + calculate(system).potential;
    LangevinIntegrator integrator;
    double largestError = 0.0;
    for (int i = 0; i < 3000; ++i) {
        const auto field = integrator.step(system, .05, 0.0, false, calculate);
        largestError = std::max(largestError, std::abs(system.kineticEnergy() + field.potential - initial));
    }
    require(largestError/initial < .001, "isolated molecule energy conservation");
    close(integrator.exchangedHeat(), 0.0, 0.0, "no heat in isolated simulation");
}

void fermions() {
    ElectronGas gas;
    constexpr double count = 210.0, volume = 1000.0;
    constexpr double kb = 1.380649e-23;
    const auto cold = gas.evaluate(count, volume, 0.0);
    const double n = count/(volume*1.e-30);
    close(cold.pressurePa, .4*n*kb*cold.fermiTemperatureK, cold.pressurePa*1.e-12, "zero-temperature Fermi pressure");
    const auto hot = gas.evaluate(count, volume, cold.fermiTemperatureK*1.e5);
    close(hot.pressurePa, n*kb*cold.fermiTemperatureK*1.e5, hot.pressurePa*1.e-12, "classical electron pressure limit");
    const double temperature = cold.fermiTemperatureK*.5;
    const auto middle = gas.evaluate(count, volume, temperature);
    require(middle.pressurePa > n*kb*temperature, "Fermi pressure exceeds classical ideal pressure");
    const double dv = volume*1.e-4;
    const double pressure = -(gas.evaluate(count, volume + dv, temperature).freeEnergyJ -
        gas.evaluate(count, volume - dv, temperature).freeEnergyJ)/(2*dv*1.e-30);
    close(pressure, middle.pressurePa, middle.pressurePa*1.e-5, "electron pressure = -dF/dV");
    for (const double theta : {.04999, .05001}) {
        const auto state = gas.evaluate(count, volume, cold.fermiTemperatureK*theta);
        require(state.pressurePa > cold.pressurePa, "finite-temperature Fermi pressure");
    }
    require(middle.screeningLengthA > 0.0 && std::isfinite(middle.screeningLengthA), "finite screening length");
}

void structureAndColors() {
    auto ice = InitialConditions::create(Experiment::IceSeed);
    auto vapor = InitialConditions::create(Experiment::Vapor);
    auto mixed = InitialConditions::create(Experiment::LiquidVaporSlab);
    StructureAnalyzer analyzer;
    analyzer.analyze(ice, 0, 180, .1, 0, ModelKind::MolecularWater);
    StructureObservation observation;
    for (int i = 0; i < 6; ++i) observation = analyzer.analyze(ice, 200.0 + i, 180, .1, 0, ModelKind::MolecularWater);
    require(observation.crystallineFraction > .9, "crystalline ice seed recognized from geometry and mobility");
    StructureAnalyzer gasAnalyzer, mixedAnalyzer;
    const auto gas = gasAnalyzer.analyze(vapor, 0, 500, .1, 0, ModelKind::MolecularWater);
    require(gas.vaporFraction > .9, "dilute vapor recognized from neighbors");
    const auto slab = mixedAnalyzer.analyze(mixed, 0, 400, .1, 0, ModelKind::MolecularWater);
    require(slab.structure == ObservedStructure::Heterogeneous, "dense and dilute regions recognized together");
    const auto blue = StateAppearance::forState(LocalState::LiquidLike);
    const auto yellow = StateAppearance::forState(LocalState::VaporLike);
    require(blue.red != yellow.red && blue.blue != yellow.blue, "phase colors visibly differ");
}

void experimentsMove() {
    for (int i = 0; i < 7; ++i) {
        SimulationEngine engine;
        engine.reset(static_cast<Experiment>(i));
        const auto before = engine.system().particles[0].positionA;
        engine.advance(256);
        require(engine.system().finite(), "finite experiment " + std::to_string(i + 1));
        require(norm(engine.system().particles[0].positionA - before) > 1.e-7, "particles move in experiment " + std::to_string(i + 1));
        require(engine.report().timeFs > 0 && std::isfinite(engine.report().pressureMPa), "time and pressure measured");
        require(engine.report().observation.particleStates.size() == engine.system().particles.size(), "one color state per particle");
        if (i == 5) require(engine.report().field.electronTemperatureRatio > 1.0, "hot plasma is nondegenerate");
        if (i == 6) require(engine.report().field.electronTemperatureRatio < 1.0, "dense electrons are degenerate");
    }
}

void barostatIdealGas() {
    ParticleSystem system;
    constexpr int count = 12;
    constexpr double temperature = 300.0, pressure = .1;
    const double expectedVolume = (count + 1.0)*units::boltzmann*temperature/pressure*units::pressureToMPa;
    system.box = PeriodicBox(std::cbrt(expectedVolume));
    for (int i = 0; i < count; ++i) system.particles.push_back({{double(i),0,0},{},{},{},1,0,Element::Hydrogen});
    VolumeController controller;
    ForceEvaluation field;
    const auto zero = [](ParticleSystem&) { return ForceEvaluation{}; };
    double total = 0.0;
    constexpr int burnIn = 20000, samples = 200000;
    for (int i = 0; i < burnIn + samples; ++i) {
        controller.attempt(system, field, temperature, pressure, zero);
        if (i >= burnIn) total += system.box.volumeA3();
    }
    close(total/samples, expectedVolume, expectedVolume*.08, "NPT ideal gas mean volume");
}
}

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> checks{
        {"periodic box and molecular compression", periodicAndScaling},
        {"water forces and energy gradient", waterForces},
        {"screened ion forces and energy gradient", plasmaForces},
        {"Ewald Madelung reference and invariance", ewaldMadelung},
        {"Verlet energy conservation", verletEnergy},
        {"canonical heat bath", canonicalBath},
        {"Fermi gas limits and thermodynamics", fermions},
        {"structure recognition and colors", structureAndColors},
        {"motion in all seven experiments", experimentsMove},
        {"Monte Carlo pressure ensemble", barostatIdealGas}};
    int failed = 0;
    for (const auto& check : checks) {
        try { check.second(); std::cout << "PASS " << check.first << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL " << check.first << ": " << error.what() << '\n'; }
    }
    return failed == 0 ? 0 : 1;
}
