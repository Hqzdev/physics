#include "ElectronGas.h"
#include "../core/ParticleSystem.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace physics {
namespace {
constexpr double hbar = 1.054571817e-34;
constexpr double mass = 9.1093837139e-31;
constexpr double kb = 1.380649e-23;
constexpr double charge = 1.602176634e-19;
constexpr double epsilon0 = 8.8541878128e-12;

struct Integrals { double density = 0.0, pressure = 0.0, response = 0.0; };

Integrals integrate(double eta) {
    // x = p / sqrt(2 m k T). Интеграл обрезается там, где хвост < exp(-40).
    constexpr int intervals = 512;
    const double end = std::sqrt(std::max(eta, 0.0) + 40.0);
    const double step = end / intervals;
    Integrals result;
    for (int i = 0; i <= intervals; ++i) {
        const double x = i * step;
        const double y = x*x - eta;
        const double f = y > 40.0 ? std::exp(-y) : 1.0 / (std::exp(y) + 1.0);
        const double weight = i == 0 || i == intervals ? 1.0 : i%2 ? 4.0 : 2.0;
        result.density += weight*x*x*f;
        result.pressure += weight*x*x*x*x*f;
        result.response += weight*x*x*f*(1.0-f);
    }
    result.density *= step/3.0;
    result.pressure *= step/3.0;
    result.response *= step/3.0;
    return result;
}
}

ElectronState ElectronGas::evaluate(double count, double volumeA3, double temperatureK) const {
    if (count <= 0.0 || volumeA3 <= 0.0 || temperatureK < 0.0 ||
        !std::isfinite(count + volumeA3 + temperatureK)) throw std::invalid_argument("Invalid electron gas parameters");
    const double volume = volumeA3 * 1.0e-30;
    const double n = count / volume;
    const double ef = hbar*hbar/(2.0*mass) * std::pow(3.0*units::pi*units::pi*n, 2.0/3.0);
    ElectronState result;
    result.fermiTemperatureK = ef/kb;
    result.temperatureRatio = temperatureK / result.fermiTemperatureK;
    const double theta = result.temperatureRatio;
    double mu = 0.0, response = 0.0;

    if (theta < 0.05) {
        // Разложение Зоммерфельда устойчиво вблизи T=0; давление там не исчезает.
        const double pi2 = units::pi*units::pi;
        const double t2 = theta*theta, t4 = t2*t2;
        mu = ef * (1.0 - pi2*t2/12.0 - pi2*pi2*t4/80.0);
        result.pressurePa = .4*n*ef * (1.0 + 5.0*pi2*t2/12.0 - pi2*pi2*t4/16.0);
        response = 1.5*n/ef / (1.0 + pi2*t2/12.0 + 3.0*pi2*pi2*t4/80.0);
    } else {
        const double kt = kb * temperatureK;
        const double prefactor = std::pow(2.0*mass*kt, 1.5) / (units::pi*units::pi*hbar*hbar*hbar);
        if (theta > 1.0e4) {
            // Предел Максвелла — Больцмана при очень слабом вырождении.
            mu = kt * std::log(n / (prefactor * std::sqrt(units::pi)/4.0));
            result.pressurePa = n*kt;
            response = n/kt;
        } else {
            double lo = -100.0, hi = 1.0/theta + 20.0;
            for (int i = 0; i < 64; ++i) {
                const double eta = (lo+hi)*.5;
                if (prefactor*integrate(eta).density < n) lo = eta; else hi = eta;
            }
            const double eta = (lo+hi)*.5;
            const auto values = integrate(eta);
            mu = eta*kt;
            result.pressurePa = (2.0/3.0)*prefactor*kt*values.pressure;
            response = prefactor/kt*values.response;
        }
    }
    // F = mu*N - P*V; kappa^2 = e^2/epsilon0 * (dn/dmu)_T.
    result.freeEnergyJ = mu*count - result.pressurePa*volume;
    result.screeningLengthA = 1.0e10 / std::sqrt(charge*charge/epsilon0*response);
    return result;
}

} // namespace physics
