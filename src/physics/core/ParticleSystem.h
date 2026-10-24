#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace physics {

// Внутренние единицы: ангстрем, фемтосекунда, а.е.м., кДж/моль.
// «На моль» относится к энергии каждой моделируемой частицы/пары.
namespace units {
inline constexpr double pi = 3.14159265358979323846;
inline constexpr double boltzmann = 0.00831446261815324;
inline constexpr double coulomb = 1389.354576;
inline constexpr double acceleration = 1.0e-4;
inline constexpr double kinetic = 5000.0;
inline constexpr double pressureToMPa = 1660.539067173847;
inline constexpr double amuPerA3ToKgM3 = 1660.53906660;
inline constexpr double jouleToKJMol = 6.02214076e20;
}

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
};

inline double dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline double norm(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline Vec3 normalized(const Vec3& v) { return v * (1.0 / norm(v)); }

enum class Element { Hydrogen, Oxygen };
enum class ModelKind { MolecularWater, IonizedMixture };

struct Particle {
    Vec3 positionA;
    Vec3 velocityAperFs;
    Vec3 force;
    // Накапливает только физическое перемещение. Изменение объёма сюда не входит.
    Vec3 transportA;
    double massAmu = 0.0;
    double chargeE = 0.0;
    Element element = Element::Hydrogen;
};

// Периодическая ячейка: ушедшая через грань частица видна с другой стороны.
// Взаимодействие использует ближайший периодический образ.
class PeriodicBox {
public:
    explicit PeriodicBox(double sideA = 1.0);
    double sideA() const { return sideA_; }
    double volumeA3() const { return sideA_ * sideA_ * sideA_; }
    Vec3 displacement(Vec3 delta) const;
    Vec3 wrap(Vec3 position) const;
private:
    double sideA_;
};

// Открытые структуры ниже — данные расчёта. Владение и изменение системы
// инкапсулированы в SimulationEngine; наружу он отдаёт только const-ссылки.
struct ParticleSystem {
    PeriodicBox box;
    std::vector<Particle> particles;
    std::vector<std::array<std::size_t, 3>> molecules;

    double massAmu() const;
    double kineticEnergy() const;
    double translationalKineticEnergy() const;
    double temperatureK() const;
    void removeDrift();
    void rescaleVolume(double volumeRatio);
    bool finite() const;
};

// Энергия и силы вычисляются одним потенциалом, поэтому F = -grad(U).
struct ForceEvaluation {
    double potential = 0.0;
    double electronFreeEnergy = 0.0;
    double electronPressureMPa = 0.0;
    double electronTemperatureRatio = 0.0; // T_e / T_F: мера вырождения электронов.
    double screeningLengthA = 0.0;
    double freeEnergy() const { return potential + electronFreeEnergy; }
};

} // namespace physics
