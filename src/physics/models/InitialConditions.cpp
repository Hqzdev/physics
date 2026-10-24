#include "InitialConditions.h"
#include "WaterForceField.h"

#include <algorithm>
#include <stdexcept>

namespace physics {
namespace {
constexpr double oxygenMass = 15.9994;
constexpr double hydrogenMass = 1.008;
constexpr double waterMass = oxygenMass + 2.0 * hydrogenMass;

Vec3 randomUnit(std::mt19937& random) {
    std::normal_distribution<double> normal(0.0, 1.0);
    Vec3 v;
    do { v = {normal(random), normal(random), normal(random)}; } while (norm(v) < 1.0e-10);
    return normalized(v);
}
}

ExperimentSettings InitialConditions::settings(Experiment e) {
    switch (e) {
    case Experiment::Liquid: return {ModelKind::MolecularWater, 300.0, 997.0, 0.1};
    case Experiment::IceSeed: return {ModelKind::MolecularWater, 180.0, 920.0, 0.1};
    case Experiment::Vapor: return {ModelKind::MolecularWater, 500.0, 3.0, 0.1};
    case Experiment::LiquidVaporSlab: return {ModelKind::MolecularWater, 400.0, 623.125, 0.1};
    case Experiment::HotFluid: return {ModelKind::MolecularWater, 800.0, 300.0, 30.0};
    case Experiment::HotPlasma: return {ModelKind::IonizedMixture, 2.0e7, 1000.0, 1.0e7};
    case Experiment::DensePlasma: return {ModelKind::IonizedMixture, 1.0e6, 1.0e6, 1.0e10};
    }
    throw std::invalid_argument("Unknown experiment");
}

void InitialConditions::addWater(ParticleSystem& system, Vec3 o, Vec3 h1, Vec3 h2) {
    const auto base = system.particles.size();
    system.particles.push_back({o, {}, {}, {}, oxygenMass, -0.82, Element::Oxygen});
    system.particles.push_back({o + h1, {}, {}, {}, hydrogenMass, 0.41, Element::Hydrogen});
    system.particles.push_back({o + h2, {}, {}, {}, hydrogenMass, 0.41, Element::Hydrogen});
    system.molecules.push_back({base, base + 1, base + 2});
}

void InitialConditions::thermalVelocities(ParticleSystem& system, double temperatureK, std::mt19937& random) {
    std::normal_distribution<double> normal(0.0, 1.0);
    for (auto& p : system.particles) {
        const double scale = std::sqrt(units::boltzmann * temperatureK * units::acceleration / p.massAmu);
        p.velocityAperFs = Vec3{normal(random), normal(random), normal(random)} * scale;
    }
    system.removeDrift();
    const double correction = std::sqrt(temperatureK / system.temperatureK());
    for (auto& p : system.particles) p.velocityAperFs = p.velocityAperFs * correction;
}

ParticleSystem InitialConditions::create(Experiment experiment) {
    const auto config = settings(experiment);
    ParticleSystem system;
    std::mt19937 random(20261024);
    const double moleculeCount = config.model == ModelKind::IonizedMixture ? 21.0 :
        experiment == Experiment::LiquidVaporSlab ? 40.0 : 64.0;
    const double side = std::cbrt(moleculeCount * waterMass * units::amuPerA3ToKgM3 / config.densityKgM3);
    system.box = PeriodicBox(side);

    if (config.model == ModelKind::IonizedMixture) {
        // 21 ядер O и 42 ядер H; электроны описывает нейтрализующий фон.
        for (int i = 0; i < 63; ++i) {
            const bool oxygen = i % 3 == 0;
            Vec3 p{(i%4 + 0.5)*side/4.0, ((i/4)%4 + 0.5)*side/4.0, (i/16 + 0.5)*side/4.0};
            p += randomUnit(random) * (0.02 * side / 4.0);
            system.particles.push_back({p, {}, {}, {}, oxygen ? oxygenMass : hydrogenMass,
                oxygen ? 8.0 : 1.0, oxygen ? Element::Oxygen : Element::Hydrogen});
        }
    } else if (experiment == Experiment::IceSeed) {
        // Кислородная алмазная сетка — начальная конфигурация кубического льда Ic.
        // Эйлерова ориентация четырёхсвязного графа даёт два исходящих ребра
        // у каждого O: два H у молекулы и один H на каждом ребре O...O.
        const Vec3 basis[] = {{0,0,0},{0,.5,.5},{.5,0,.5},{.5,.5,0},
            {.25,.25,.25},{.25,.75,.75},{.75,.25,.75},{.75,.75,.25}};
        std::vector<Vec3> positions;
        for (int x = 0; x < 2; ++x) for (int y = 0; y < 2; ++y) for (int z = 0; z < 2; ++z)
            for (const auto b : basis) positions.push_back((Vec3{double(x),double(y),double(z)} + b) * (side/2.0));
        std::vector<std::vector<std::size_t>> edges(64), outgoing(64);
        bool used[64][64]{};
        for (std::size_t i = 0; i < 64; ++i) for (std::size_t j = i + 1; j < 64; ++j)
            if (norm(system.box.displacement(positions[j] - positions[i])) < side * 0.23) {
                edges[i].push_back(j); edges[j].push_back(i);
            }
        std::vector<std::size_t> stack{0};
        while (!stack.empty()) {
            const auto i = stack.back();
            auto neighbor = std::find_if(edges[i].begin(), edges[i].end(), [&](auto j) { return !used[i][j]; });
            if (neighbor == edges[i].end()) { stack.pop_back(); continue; }
            const auto j = *neighbor;
            used[i][j] = used[j][i] = true;
            outgoing[i].push_back(j);
            stack.push_back(j);
        }
        for (std::size_t i = 0; i < 64; ++i) {
            if (edges[i].size() != 4 || outgoing[i].size() != 2) throw std::runtime_error("Invalid ice topology");
            const Vec3 a = normalized(system.box.displacement(positions[outgoing[i][0]] - positions[i]));
            const Vec3 b = normalized(system.box.displacement(positions[outgoing[i][1]] - positions[i]));
            addWater(system, positions[i], a * WaterForceField::bondLengthA, b * WaterForceField::bondLengthA);
        }
    } else {
        const int count = static_cast<int>(moleculeCount);
        for (int i = 0; i < count; ++i) {
            Vec3 o{(i%4 + .5)*side/4.0, ((i/4)%4 + .5)*side/4.0, (i/16 + .5)*side/4.0};
            if (experiment == Experiment::LiquidVaporSlab && i >= 32) {
                const int gas = i - 32;
                o = {(gas%2 + .5)*side/2.0, ((gas/2)%2 + .5)*side/2.0,
                    (0.625 + 0.25*(gas/4))*side};
            }
            const Vec3 bisector = randomUnit(random);
            const Vec3 helper = std::abs(bisector.z) < .8 ? Vec3{0,0,1} : Vec3{0,1,0};
            const Vec3 tangent = normalized(cross(bisector, helper));
            const double half = WaterForceField::angleRad * .5;
            addWater(system, o,
                (bisector*std::cos(half) + tangent*std::sin(half)) * WaterForceField::bondLengthA,
                (bisector*std::cos(half) - tangent*std::sin(half)) * WaterForceField::bondLengthA);
        }
    }
    thermalVelocities(system, config.temperatureK, random);
    return system;
}

} // namespace physics
