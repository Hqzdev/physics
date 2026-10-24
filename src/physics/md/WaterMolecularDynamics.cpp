#include "WaterMolecularDynamics.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <utility>

namespace watermd {
namespace {
constexpr double BOLTZMANN_KJ_PER_MOL_K = 0.008314462618;
constexpr double COULOMB_KJ_ANGSTROM_PER_MOL_E2 = 1389.354576;
constexpr double PRESSURE_PA_PER_KJ_MOL_ANGSTROM3 = 1.66053906660e9;
constexpr double ANGSTROM3_TO_KG_M3_FACTOR = 18.01528 * 1.66053906660 * 1000.0;
constexpr double OXYGEN_MASS_AMU = 15.9994;
constexpr double HYDROGEN_MASS_AMU = 1.008;
constexpr double OXYGEN_CHARGE_E = -0.82;
constexpr double HYDROGEN_CHARGE_E = 0.41;
constexpr double OXYGEN_SIGMA_A = 3.16549;
constexpr double OXYGEN_EPSILON_KJ_PER_MOL = 0.6502;
constexpr double OH_BOND_LENGTH_A = 1.012;
constexpr double HOH_ANGLE_RAD = 113.24 * 3.14159265358979323846 / 180.0;
constexpr double BOND_FORCE_KJ_PER_MOL_A2 = 4431.13;
constexpr double ANGLE_FORCE_KJ_PER_MOL_RAD2 = 317.57;
constexpr double INTERACTION_CUTOFF_A = 9.0;
constexpr double TIME_STEP_FS = 0.25;
constexpr double TARGET_DENSITY_KG_M3 = 997.0;

double dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

double length(const Vec3& value) {
    return std::sqrt(dot(value, value));
}

Vec3 cross(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalized(const Vec3& value) {
    const double size = length(value);
    return size > 1e-12 ? value * (1.0 / size) : Vec3{1.0, 0.0, 0.0};
}

double clampUnit(double value) {
    return std::max(-1.0, std::min(1.0, value));
}

} // namespace

Vec3 Vec3::operator+(const Vec3& other) const {
    return {x + other.x, y + other.y, z + other.z};
}

Vec3 Vec3::operator-(const Vec3& other) const {
    return {x - other.x, y - other.y, z - other.z};
}

Vec3 Vec3::operator*(double scale) const {
    return {x * scale, y * scale, z * scale};
}

Vec3& Vec3::operator+=(const Vec3& other) {
    x += other.x;
    y += other.y;
    z += other.z;
    return *this;
}

Vec3& Vec3::operator-=(const Vec3& other) {
    x -= other.x;
    y -= other.y;
    z -= other.z;
    return *this;
}

WaterMolecularDynamics::WaterMolecularDynamics()
    : timeStepFs_(TIME_STEP_FS) {
    initializeWater();
    double virial = 0.0;
    calculateForces(virial);
    measure(virial);
}

void WaterMolecularDynamics::setTargetTemperature(double temperatureK) {
    targetTemperatureK_ = std::max(1.0, std::min(temperatureK, 600.0));
}

void WaterMolecularDynamics::setTargetPressure(double pressureMPa) {
    targetPressureMPa_ = std::max(0.001, std::min(pressureMPa, 100.0));
}

void WaterMolecularDynamics::advance(unsigned int integrationSteps) {
    for (unsigned int step = 0; step < integrationSteps; ++step) {
        integrateStep();
    }

    double virial = 0.0;
    calculateForces(virial);
    measure(virial);
}

const std::vector<WaterMolecule>& WaterMolecularDynamics::molecules() const {
    return molecules_;
}

double WaterMolecularDynamics::boxLengthA() const {
    return boxLengthA_;
}

const Measurements& WaterMolecularDynamics::measurements() const {
    return measurements_;
}

void WaterMolecularDynamics::initializeWater() {
    constexpr int MOLECULES_PER_SIDE = 4;
    constexpr std::size_t MOLECULE_COUNT =
        MOLECULES_PER_SIDE * MOLECULES_PER_SIDE * MOLECULES_PER_SIDE;

    const double volumeA3 =
        MOLECULE_COUNT * ANGSTROM3_TO_KG_M3_FACTOR / TARGET_DENSITY_KG_M3;
    boxLengthA_ = std::cbrt(volumeA3);
    const double spacingA = boxLengthA_ / MOLECULES_PER_SIDE;

    std::mt19937 generator(20261022);
    std::normal_distribution<double> normal(0.0, 1.0);
    std::uniform_real_distribution<double> uniform(-1.0, 1.0);
    molecules_.reserve(MOLECULE_COUNT);

    for (int x = 0; x < MOLECULES_PER_SIDE; ++x) {
        for (int y = 0; y < MOLECULES_PER_SIDE; ++y) {
            for (int z = 0; z < MOLECULES_PER_SIDE; ++z) {
                const Vec3 oxygenPosition{
                    (x + 0.5) * spacingA,
                    (y + 0.5) * spacingA,
                    (z + 0.5) * spacingA
                };

                Vec3 bisector = normalized({uniform(generator), uniform(generator), uniform(generator)});
                Vec3 helper = std::abs(bisector.z) < 0.8
                    ? Vec3{0.0, 0.0, 1.0}
                    : Vec3{0.0, 1.0, 0.0};
                Vec3 side = normalized(cross(bisector, helper));
                const double halfAngle = HOH_ANGLE_RAD * 0.5;
                const Vec3 firstHydrogenOffset =
                    (bisector * std::cos(halfAngle) + side * std::sin(halfAngle)) * OH_BOND_LENGTH_A;
                const Vec3 secondHydrogenOffset =
                    (bisector * std::cos(halfAngle) - side * std::sin(halfAngle)) * OH_BOND_LENGTH_A;

                WaterMolecule molecule{};
                molecule.atoms[0] = {
                    oxygenPosition, {}, {}, OXYGEN_MASS_AMU, OXYGEN_CHARGE_E, true
                };
                molecule.atoms[1] = {
                    oxygenPosition + firstHydrogenOffset, {}, {},
                    HYDROGEN_MASS_AMU, HYDROGEN_CHARGE_E, false
                };
                molecule.atoms[2] = {
                    oxygenPosition + secondHydrogenOffset, {}, {},
                    HYDROGEN_MASS_AMU, HYDROGEN_CHARGE_E, false
                };

                for (Atom& atom : molecule.atoms) {
                    const double velocityScale = std::sqrt(
                        BOLTZMANN_KJ_PER_MOL_K * targetTemperatureK_ /
                        (10000.0 * atom.massAmu));
                    atom.velocityAperFs = {
                        normal(generator) * velocityScale,
                        normal(generator) * velocityScale,
                        normal(generator) * velocityScale
                    };
                    wrapIntoBox(atom.positionA);
                }

                molecules_.push_back(molecule);
            }
        }
    }

    // Убираем общее движение всей коробки, чтобы оно не считалось нагревом.
    Vec3 totalMomentum{};
    double totalMass = 0.0;
    for (const WaterMolecule& molecule : molecules_) {
        for (const Atom& atom : molecule.atoms) {
            totalMomentum += atom.velocityAperFs * atom.massAmu;
            totalMass += atom.massAmu;
        }
    }
    const Vec3 centerVelocity = totalMomentum * (1.0 / totalMass);
    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.velocityAperFs -= centerVelocity;
        }
    }
}

void WaterMolecularDynamics::calculateForces(double& virialKJperMol) {
    virialKJperMol = 0.0;
    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.forceKJperMolA = {};
        }
    }

    calculateBondAndAngleForces(virialKJperMol);
    calculateIntermolecularForces(virialKJperMol);
}

void WaterMolecularDynamics::calculateBondAndAngleForces(double& virialKJperMol) {
    for (WaterMolecule& molecule : molecules_) {
        Atom& oxygen = molecule.atoms[0];
        Atom& firstHydrogen = molecule.atoms[1];
        Atom& secondHydrogen = molecule.atoms[2];

        const Vec3 firstBond = minimumImage(firstHydrogen.positionA - oxygen.positionA);
        const Vec3 secondBond = minimumImage(secondHydrogen.positionA - oxygen.positionA);
        const double firstLength = length(firstBond);
        const double secondLength = length(secondBond);

        if (firstLength > 1e-12) {
            const Vec3 force = firstBond *
                (-BOND_FORCE_KJ_PER_MOL_A2 * (firstLength - OH_BOND_LENGTH_A) / firstLength);
            firstHydrogen.forceKJperMolA += force;
            oxygen.forceKJperMolA -= force;
            virialKJperMol += dot(firstBond, force);
        }
        if (secondLength > 1e-12) {
            const Vec3 force = secondBond *
                (-BOND_FORCE_KJ_PER_MOL_A2 * (secondLength - OH_BOND_LENGTH_A) / secondLength);
            secondHydrogen.forceKJperMolA += force;
            oxygen.forceKJperMolA -= force;
            virialKJperMol += dot(secondBond, force);
        }

        if (firstLength <= 1e-12 || secondLength <= 1e-12) {
            continue;
        }

        const Vec3 firstUnit = firstBond * (1.0 / firstLength);
        const Vec3 secondUnit = secondBond * (1.0 / secondLength);
        const double cosine = clampUnit(dot(firstUnit, secondUnit));
        const double angle = std::acos(cosine);
        const double sine = std::sqrt(std::max(1e-12, 1.0 - cosine * cosine));
        const double angleDifference = angle - HOH_ANGLE_RAD;

        const Vec3 firstAngleForce =
            (secondUnit - firstUnit * cosine) *
            (ANGLE_FORCE_KJ_PER_MOL_RAD2 * angleDifference / (firstLength * sine));
        const Vec3 secondAngleForce =
            (firstUnit - secondUnit * cosine) *
            (ANGLE_FORCE_KJ_PER_MOL_RAD2 * angleDifference / (secondLength * sine));

        firstHydrogen.forceKJperMolA += firstAngleForce;
        secondHydrogen.forceKJperMolA += secondAngleForce;
        oxygen.forceKJperMolA -= firstAngleForce + secondAngleForce;
        virialKJperMol += dot(firstBond, firstAngleForce) +
            dot(secondBond, secondAngleForce);
    }
}

void WaterMolecularDynamics::calculateIntermolecularForces(double& virialKJperMol) {
    // Для периодической коробки радиус действия должен быть меньше половины её стороны.
    const double cutoffA = std::min(INTERACTION_CUTOFF_A, 0.49 * boxLengthA_);
    const double cutoffSquared = cutoffA * cutoffA;
    const double cutoffSigmaRatioSquared =
        (OXYGEN_SIGMA_A * OXYGEN_SIGMA_A) / cutoffSquared;
    const double cutoffSigmaRatioSixth =
        cutoffSigmaRatioSquared * cutoffSigmaRatioSquared * cutoffSigmaRatioSquared;
    const double cutoffLennardJonesForceMagnitude =
        24.0 * OXYGEN_EPSILON_KJ_PER_MOL / cutoffA *
        cutoffSigmaRatioSixth * (2.0 * cutoffSigmaRatioSixth - 1.0);

    for (std::size_t moleculeA = 0; moleculeA < molecules_.size(); ++moleculeA) {
        for (std::size_t moleculeB = moleculeA + 1; moleculeB < molecules_.size(); ++moleculeB) {
            for (Atom& atomA : molecules_[moleculeA].atoms) {
                for (Atom& atomB : molecules_[moleculeB].atoms) {
                    const Vec3 displacement = minimumImage(atomA.positionA - atomB.positionA);
                    const double distanceSquared = dot(displacement, displacement);
                    if (distanceSquared >= cutoffSquared || distanceSquared < 1e-10) {
                        continue;
                    }

                    const double distance = std::sqrt(distanceSquared);
                    double forceScale = COULOMB_KJ_ANGSTROM_PER_MOL_E2 *
                        atomA.chargeE * atomB.chargeE *
                        (1.0 / (distanceSquared * distance) -
                         1.0 / (cutoffSquared * distance));

                    if (atomA.isOxygen && atomB.isOxygen) {
                        const double ratioSquared =
                            (OXYGEN_SIGMA_A * OXYGEN_SIGMA_A) / distanceSquared;
                        const double ratioSixth =
                            ratioSquared * ratioSquared * ratioSquared;
                        const double lennardJonesScale =
                            24.0 * OXYGEN_EPSILON_KJ_PER_MOL / distanceSquared *
                                ratioSixth * (2.0 * ratioSixth - 1.0)
                            - cutoffLennardJonesForceMagnitude / distance;
                        forceScale += lennardJonesScale;
                    }

                    const Vec3 force = displacement * forceScale;
                    atomA.forceKJperMolA += force;
                    atomB.forceKJperMolA -= force;
                    virialKJperMol += dot(displacement, force);
                }
            }
        }
    }
}

void WaterMolecularDynamics::integrateStep() {
    double virial = 0.0;
    calculateForces(virial);

    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.velocityAperFs += atom.forceKJperMolA *
                (0.5e-4 * timeStepFs_ / atom.massAmu);
            atom.positionA += atom.velocityAperFs * timeStepFs_;
            wrapIntoBox(atom.positionA);
        }
    }

    calculateForces(virial);
    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.velocityAperFs += atom.forceKJperMolA *
                (0.5e-4 * timeStepFs_ / atom.massAmu);
        }
    }

    applyThermostat();
    applyBarostat(virial);
}

void WaterMolecularDynamics::applyThermostat() {
    double kineticEnergy = 0.0;
    for (const WaterMolecule& molecule : molecules_) {
        for (const Atom& atom : molecule.atoms) {
            kineticEnergy += 5000.0 * atom.massAmu *
                dot(atom.velocityAperFs, atom.velocityAperFs);
        }
    }

    const double degreesOfFreedom = 3.0 * molecules_.size() * 3.0 - 3.0;
    const double currentTemperature =
        2.0 * kineticEnergy / (degreesOfFreedom * BOLTZMANN_KJ_PER_MOL_K);
    if (currentTemperature <= 1e-12) {
        return;
    }

    // Мягко обмениваемся энергией с термостатом, не назначая фазу воды.
    constexpr double THERMOSTAT_TIME_FS = 100.0;
    const double coupling = std::min(1.0, timeStepFs_ / THERMOSTAT_TIME_FS);
    const double scale = std::sqrt(std::max(
        0.0, 1.0 + coupling * (targetTemperatureK_ / currentTemperature - 1.0)));
    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.velocityAperFs = atom.velocityAperFs * scale;
        }
    }
}

void WaterMolecularDynamics::applyBarostat(double virialKJperMol) {
    double kineticEnergy = 0.0;
    for (const WaterMolecule& molecule : molecules_) {
        for (const Atom& atom : molecule.atoms) {
            kineticEnergy += 5000.0 * atom.massAmu *
                dot(atom.velocityAperFs, atom.velocityAperFs);
        }
    }

    const double volumeA3 = boxLengthA_ * boxLengthA_ * boxLengthA_;
    const double currentPressureMPa =
        ((2.0 * kineticEnergy + virialKJperMol) / (3.0 * volumeA3)) *
        PRESSURE_PA_PER_KJ_MOL_ANGSTROM3 / 1.0e6;

    // Слабое баростатное управление плавно меняет размер ячейки по измеренному давлению.
    constexpr double WATER_COMPRESSIBILITY_PER_PA = 4.6e-10;
    constexpr double BAROSTAT_TIME_FS = 2000.0;
    const double volumeChange =
        (timeStepFs_ / BAROSTAT_TIME_FS) * WATER_COMPRESSIBILITY_PER_PA *
        (currentPressureMPa - targetPressureMPa_) * 1.0e6;
    const double boundedVolumeChange = std::max(-1e-4, std::min(1e-4, volumeChange));
    const double lengthScale = std::cbrt(1.0 + boundedVolumeChange);
    if (std::abs(lengthScale - 1.0) < 1e-12) {
        return;
    }

    boxLengthA_ *= lengthScale;
    for (WaterMolecule& molecule : molecules_) {
        for (Atom& atom : molecule.atoms) {
            atom.positionA = atom.positionA * lengthScale;
            wrapIntoBox(atom.positionA);
        }
    }
}

void WaterMolecularDynamics::measure(double virialKJperMol) {
    double kineticEnergy = 0.0;
    std::vector<Vec3> oxygenPositions;
    oxygenPositions.reserve(molecules_.size());

    for (const WaterMolecule& molecule : molecules_) {
        oxygenPositions.push_back(molecule.atoms[0].positionA);
        for (const Atom& atom : molecule.atoms) {
            kineticEnergy += 5000.0 * atom.massAmu *
                dot(atom.velocityAperFs, atom.velocityAperFs);
        }
    }

    const double degreesOfFreedom = 3.0 * molecules_.size() * 3.0 - 3.0;
    measurements_.temperatureK =
        2.0 * kineticEnergy / (degreesOfFreedom * BOLTZMANN_KJ_PER_MOL_K);

    const double volumeA3 = boxLengthA_ * boxLengthA_ * boxLengthA_;
    measurements_.pressureMPa =
        ((2.0 * kineticEnergy + virialKJperMol) / (3.0 * volumeA3)) *
        PRESSURE_PA_PER_KJ_MOL_ANGSTROM3 / 1.0e6;
    measurements_.densityKgM3 =
        molecules_.size() * ANGSTROM3_TO_KG_M3_FACTOR / volumeA3;

    double totalOrder = 0.0;
    double totalNeighbors = 0.0;
    for (std::size_t i = 0; i < oxygenPositions.size(); ++i) {
        std::vector<std::pair<double, Vec3>> neighbors;
        neighbors.reserve(oxygenPositions.size() - 1);
        for (std::size_t j = 0; j < oxygenPositions.size(); ++j) {
            if (i == j) {
                continue;
            }
            const Vec3 displacement = minimumImage(oxygenPositions[j] - oxygenPositions[i]);
            const double distanceSquared = dot(displacement, displacement);
            if (distanceSquared > 1e-12) {
                neighbors.push_back({distanceSquared, displacement});
                if (distanceSquared < 3.5 * 3.5) {
                    totalNeighbors += 1.0;
                }
            }
        }

        const std::size_t nearestCount = std::min<std::size_t>(4, neighbors.size());
        std::partial_sort(
            neighbors.begin(), neighbors.begin() + nearestCount, neighbors.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

        double localOrder = 0.0;
        unsigned int angleCount = 0;
        for (std::size_t a = 0; a < nearestCount; ++a) {
            for (std::size_t b = a + 1; b < nearestCount; ++b) {
                const Vec3 va = normalized(neighbors[a].second);
                const Vec3 vb = normalized(neighbors[b].second);
                const double difference = dot(va, vb) + 1.0 / 3.0;
                localOrder += difference * difference;
                ++angleCount;
            }
        }
        if (angleCount > 0) {
            totalOrder += 1.0 - (3.0 / 8.0) * localOrder;
        }
    }

    measurements_.tetrahedralOrder = oxygenPositions.empty()
        ? 0.0
        : totalOrder / oxygenPositions.size();
    measurements_.averageOxygenNeighbors = oxygenPositions.empty()
        ? 0.0
        : totalNeighbors / oxygenPositions.size();
}

void WaterMolecularDynamics::wrapIntoBox(Vec3& position) const {
    position.x -= boxLengthA_ * std::floor(position.x / boxLengthA_);
    position.y -= boxLengthA_ * std::floor(position.y / boxLengthA_);
    position.z -= boxLengthA_ * std::floor(position.z / boxLengthA_);
}

Vec3 WaterMolecularDynamics::minimumImage(Vec3 displacement) const {
    displacement.x -= boxLengthA_ * std::round(displacement.x / boxLengthA_);
    displacement.y -= boxLengthA_ * std::round(displacement.y / boxLengthA_);
    displacement.z -= boxLengthA_ * std::round(displacement.z / boxLengthA_);
    return displacement;
}

} // namespace watermd
