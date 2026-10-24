#pragma once

#include "../core/ParticleSystem.h"
#include <deque>

namespace physics {

// Метки наблюдения, а не команды физическому движку.
// «Похоже на лёд» не является доказательством конкретной равновесной фазы льда.
enum class LocalState { Unknown, IceLike, LiquidLike, VaporLike, Interface, HotFluid, Plasma, DegeneratePlasma };
enum class ObservedStructure { MolecularFluid, IceLike, VaporLike, Heterogeneous, HotFluidCandidate, Plasma, DegeneratePlasma };

struct StructureObservation {
    std::vector<LocalState> particleStates;
    ObservedStructure structure = ObservedStructure::MolecularFluid;
    double tetrahedralOrder = 0.0;
    double oxygenNeighbors = 0.0;
    double crystallineFraction = 0.0;
    double vaporFraction = 0.0;
    double oxygenDisplacementA2 = 0.0;
    double observationWindowFs = 0.0;
};

// Геометрия ближайших соседей и перемещение за окно наблюдения.
// Анализатор не хранит ссылок, способных изменить ParticleSystem.
class StructureAnalyzer {
public:
    StructureObservation analyze(const ParticleSystem& system, double timeFs,
        double averagedTemperatureK, double averagedPressureMPa, double electronTemperatureRatio, ModelKind model);
private:
    struct Sample { double timeFs; std::vector<Vec3> positions; };
    std::deque<Sample> history_;
    std::vector<LocalState> stable_, pending_;
    std::vector<unsigned int> streak_;
};

} // namespace physics
