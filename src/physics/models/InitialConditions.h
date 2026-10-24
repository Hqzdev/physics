#pragma once

#include "../core/ParticleSystem.h"
#include <random>

namespace physics {

// Эксперимент задаёт исходную конфигурацию. Дальнейшее состояние
// не закрепляется за ней: частицы продолжают двигаться по одним уравнениям.
enum class Experiment { Liquid, IceSeed, Vapor, LiquidVaporSlab, HotFluid, HotPlasma, DensePlasma };

struct ExperimentSettings {
    ModelKind model = ModelKind::MolecularWater;
    double temperatureK = 300.0;
    double densityKgM3 = 997.0;
    double pressureMPa = 0.1;
};

class InitialConditions {
public:
    static ExperimentSettings settings(Experiment experiment);
    static ParticleSystem create(Experiment experiment);
private:
    static void addWater(ParticleSystem& system, Vec3 oxygen, Vec3 h1, Vec3 h2);
    static void thermalVelocities(ParticleSystem& system, double temperatureK, std::mt19937& random);
};

} // namespace physics
