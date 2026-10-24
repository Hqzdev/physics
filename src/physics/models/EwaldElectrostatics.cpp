#include "EwaldElectrostatics.h"

#include <array>
#include <complex>
#include <stdexcept>

namespace physics {

EwaldElectrostatics::EwaldElectrostatics(int radius, double alphaLength)
    : reciprocalRadius_(radius), alphaLength_(alphaLength) {
    if (radius < 1 || radius > 12 || !std::isfinite(alphaLength) || alphaLength <= 0.0)
        throw std::invalid_argument("Invalid Ewald accuracy parameters");
}

double EwaldElectrostatics::accumulate(ParticleSystem& system) const {
    const double length = system.box.sideA();
    const double alpha = alphaLength_/length;
    const double rc = .49*length;
    const double rootPi = std::sqrt(units::pi);
    double energy = 0.0, netCharge = 0.0, squaredCharges = 0.0;
    for (const auto& p : system.particles) { netCharge += p.chargeE; squaredCharges += p.chargeE*p.chargeE; }
    if (std::abs(netCharge) > 1.e-9) throw std::invalid_argument("Water Ewald cell must be neutral");

    for (std::size_t i = 0; i < system.particles.size(); ++i) {
        for (std::size_t j = i + 1; j < system.particles.size(); ++j) {
            auto& a = system.particles[i];
            auto& b = system.particles[j];
            const Vec3 delta = system.box.displacement(a.positionA - b.positionA);
            const double r = norm(delta);
            if (r >= rc) continue;
            if (r < 1.e-8) throw std::runtime_error("Overlapping Ewald charges");
            const double charge = units::coulomb*a.chargeE*b.chargeE;
            const double screened = std::erfc(alpha*r);
            energy += charge*screened/r;
            const Vec3 force = delta * (charge*(screened/(r*r*r) +
                2.0*alpha/rootPi*std::exp(-alpha*alpha*r*r)/(r*r)));
            a.force += force;
            b.force -= force;
        }
    }

    // exp(i*n*x) строим рекуррентно. На каждую ось атома нужен один
    // sin/cos, а не тригонометрический вызов на каждую обратную волну.
    using Complex = std::complex<double>;
    using Axes = std::array<std::vector<Complex>, 3>;
    std::vector<Axes> waves(system.particles.size());
    const int radius = reciprocalRadius_;
    const double k0 = 2.0*units::pi/length;
    for (std::size_t i = 0; i < system.particles.size(); ++i) {
        const auto p = system.box.wrap(system.particles[i].positionA);
        const std::array<double, 3> coords{p.x, p.y, p.z};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            auto& values = waves[i][axis];
            values.resize(static_cast<std::size_t>(2*radius + 1));
            values[static_cast<std::size_t>(radius)] = {1.0, 0.0};
            const Complex first = std::polar(1.0, k0*coords[axis]);
            for (int n = 1; n <= radius; ++n) {
                values[static_cast<std::size_t>(radius + n)] = values[static_cast<std::size_t>(radius + n - 1)]*first;
                values[static_cast<std::size_t>(radius - n)] = std::conj(values[static_cast<std::size_t>(radius + n)]);
            }
        }
    }
    std::vector<Complex> phase(system.particles.size());
    for (int h = -radius; h <= radius; ++h) for (int k = -radius; k <= radius; ++k) for (int l = -radius; l <= radius; ++l) {
        const int squared = h*h + k*k + l*l;
        // Берём одну из двух волн +/-k; множитель ниже уже удвоен.
        if (squared == 0 || squared > radius*radius || h < 0 ||
            (h == 0 && k < 0) || (h == 0 && k == 0 && l < 0)) continue;
        const Vec3 wave{h*k0, k*k0, l*k0};
        const double k2 = squared*k0*k0;
        const double coefficient = 4.0*units::pi*units::coulomb/system.box.volumeA3() *
            std::exp(-k2/(4.0*alpha*alpha))/k2;
        Complex structure{};
        for (std::size_t i = 0; i < system.particles.size(); ++i) {
            phase[i] = waves[i][0][static_cast<std::size_t>(h + radius)] *
                waves[i][1][static_cast<std::size_t>(k + radius)] * waves[i][2][static_cast<std::size_t>(l + radius)];
            structure += system.particles[i].chargeE*phase[i];
        }
        energy += coefficient*std::norm(structure);
        for (std::size_t i = 0; i < system.particles.size(); ++i)
            system.particles[i].force += wave * (2.0*coefficient*system.particles[i].chargeE*
                std::imag(phase[i]*std::conj(structure)));
    }
    energy -= units::coulomb*alpha/rootPi*squaredCharges;

    // У SPC/Fw нет кулоновского взаимодействия между тремя атомами
    // одной молекулы. Удаляем прямой вклад этих трёх пар из полной суммы.
    // Взаимодействие с периодическими образами сохраняется.
    for (const auto& molecule : system.molecules) for (std::size_t i = 0; i < 3; ++i) for (std::size_t j = i + 1; j < 3; ++j) {
        auto& a = system.particles[molecule[i]];
        auto& b = system.particles[molecule[j]];
        const Vec3 delta = system.box.displacement(a.positionA - b.positionA);
        const double r = norm(delta);
        if (r < 1.e-8) throw std::runtime_error("Collapsed molecular exclusion");
        const double charge = units::coulomb*a.chargeE*b.chargeE;
        energy -= charge/r;
        const Vec3 correction = delta * (-charge/(r*r*r));
        a.force += correction;
        b.force -= correction;
    }
    return energy;
}

} // namespace physics
