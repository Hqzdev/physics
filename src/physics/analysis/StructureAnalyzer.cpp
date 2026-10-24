#include "StructureAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace physics {
namespace {
struct Neighborhood { std::vector<std::size_t> indices; std::vector<Vec3> directions; double self = 0.0; };

double legendre6(double x) {
    const double x2 = x*x;
    return (231.0*x2*x2*x2 - 315.0*x2*x2 + 105.0*x2 - 5.0)/16.0;
}

double orientationalDot(const Neighborhood& a, const Neighborhood& b) {
    // Теорема сложения сферических гармоник позволяет сравнить локальные
    // q6-окружения через полином Лежандра без комплексных чисел.
    double value = 0.0;
    for (const auto u : a.directions) for (const auto v : b.directions)
        value += legendre6(std::clamp(dot(u, v), -1.0, 1.0));
    return value;
}
}

StructureObservation StructureAnalyzer::analyze(const ParticleSystem& system, double timeFs,
    double temperatureK, double pressureMPa, double electronTemperatureRatio, ModelKind model) {
    StructureObservation result;
    result.particleStates.resize(system.particles.size(), LocalState::Unknown);
    if (model == ModelKind::IonizedMixture) {
        // Вырождение — непрерывный режим электронного фона. Порог theta=1
        // нужен только для подписи и цвета; формулы EOS на нём не переключаются.
        const bool degenerate = electronTemperatureRatio < 1.0;
        std::fill(result.particleStates.begin(), result.particleStates.end(),
            degenerate ? LocalState::DegeneratePlasma : LocalState::Plasma);
        result.structure = degenerate ? ObservedStructure::DegeneratePlasma : ObservedStructure::Plasma;
        return result;
    }

    const auto count = system.molecules.size();
    if (count == 0) throw std::invalid_argument("Molecular analysis requires water topology");
    std::vector<Vec3> positions, transport;
    for (const auto& indices : system.molecules) {
        positions.push_back(system.particles[indices[0]].positionA);
        transport.push_back(system.particles[indices[0]].transportA);
    }
    if (history_.empty() || timeFs > history_.back().timeFs) history_.push_back({timeFs, transport});
    while (history_.size() > 1 && timeFs - history_[1].timeFs >= 200.0) history_.pop_front();
    result.observationWindowFs = timeFs - history_.front().timeFs;

    std::vector<Vec3> displacement(count);
    Vec3 drift{};
    for (std::size_t i = 0; i < count; ++i) {
        displacement[i] = transport[i] - history_.front().positions[i];
        drift += displacement[i];
    }
    drift = drift * (1.0/static_cast<double>(count));

    std::vector<Neighborhood> neighborhoods(count);
    std::vector<double> orders(count, 0.0), displacements(count, 0.0);
    for (std::size_t i = 0; i < count; ++i) {
        auto& neighbors = neighborhoods[i];
        std::vector<std::pair<double, Vec3>> sorted;
        for (std::size_t j = 0; j < count; ++j) {
            if (i == j) continue;
            const Vec3 delta = system.box.displacement(positions[j] - positions[i]);
            const double r = norm(delta);
            if (r < 1.0e-10) continue;
            sorted.push_back({r, delta*(1.0/r)});
            if (r < 3.5) { neighbors.indices.push_back(j); neighbors.directions.push_back(delta*(1.0/r)); }
        }
        neighbors.self = orientationalDot(neighbors, neighbors);
        std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        if (sorted.size() >= 4) {
            double sum = 0.0;
            for (std::size_t a = 0; a < 4; ++a) for (std::size_t b = a + 1; b < 4; ++b) {
                const double delta = dot(sorted[a].second, sorted[b].second) + 1.0/3.0;
                sum += delta*delta;
            }
            orders[i] = 1.0 - 3.0/8.0*sum;
        }
        displacements[i] = dot(displacement[i] - drift, displacement[i] - drift);
        result.oxygenDisplacementA2 += displacements[i]/static_cast<double>(count);
        result.tetrahedralOrder += orders[i]/static_cast<double>(count);
        result.oxygenNeighbors += static_cast<double>(neighbors.indices.size())/static_cast<double>(count);
    }

    if (stable_.size() != count) {
        stable_.assign(count, LocalState::Unknown);
        pending_ = stable_;
        streak_.assign(count, 0);
    }
    const bool hot = temperatureK > 647.096 && pressureMPa > 22.064;
    double denseCount = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& a = neighborhoods[i];
        unsigned int correlated = 0;
        for (const auto j : a.indices) {
            const double denominator = std::sqrt(a.self*neighborhoods[j].self);
            if (denominator > 1.0e-10 && orientationalDot(a, neighborhoods[j])/denominator > .65) ++correlated;
        }
        LocalState state = LocalState::LiquidLike;
        if (a.indices.size() <= 1) state = LocalState::VaporLike;
        else if (a.indices.size() == 2) state = LocalState::Interface;
        else if (correlated >= 3 && orders[i] > .75 && a.indices.size() <= 5) {
            if (result.observationWindowFs < 100.0) state = LocalState::Unknown;
            else if (displacements[i] < .12) state = LocalState::IceLike;
        }
        else if (hot) state = LocalState::HotFluid;

        if (pending_[i] == state) ++streak_[i]; else { pending_[i] = state; streak_[i] = 1; }
        if (stable_[i] == LocalState::Unknown || streak_[i] >= 5) stable_[i] = state;
        for (const auto atom : system.molecules[i]) result.particleStates[atom] = stable_[i];
        if (stable_[i] == LocalState::IceLike) result.crystallineFraction += 1.0/static_cast<double>(count);
        if (stable_[i] == LocalState::VaporLike) result.vaporFraction += 1.0/static_cast<double>(count);
        if (a.indices.size() >= 3) denseCount += 1.0/static_cast<double>(count);
    }
    if (result.crystallineFraction > .5) result.structure = ObservedStructure::IceLike;
    else if (result.vaporFraction > .7) result.structure = ObservedStructure::VaporLike;
    else if (result.vaporFraction > .1 && denseCount > .2) result.structure = ObservedStructure::Heterogeneous;
    else if (hot) result.structure = ObservedStructure::HotFluidCandidate;
    return result;
}

} // namespace physics
