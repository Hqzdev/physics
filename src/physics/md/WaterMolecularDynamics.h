#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace watermd {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;

    Vec3 operator+(const Vec3& other) const;
    Vec3 operator-(const Vec3& other) const;
    Vec3 operator*(double scale) const;
    Vec3& operator+=(const Vec3& other);
    Vec3& operator-=(const Vec3& other);
};

struct Atom {
    Vec3 positionA;
    Vec3 velocityAperFs;
    Vec3 forceKJperMolA;
    double massAmu = 0.0;
    double chargeE = 0.0;
    bool isOxygen = false;
};

struct WaterMolecule {
    std::array<Atom, 3> atoms;
};

struct Measurements {
    double temperatureK = 0.0;
    double pressureMPa = 0.0;
    double densityKgM3 = 0.0;
    double tetrahedralOrder = 0.0;
    double averageOxygenNeighbors = 0.0;
};

// Классическая молекулярная динамика гибкой модели типа SPC/Fw.
// Электростатика здесь приближённо обрезана по радиусу; для точных
// расчётов понадобится дальнодействующий решатель, например Ewald/PME.
// Расчёт ведётся в 3D, а окно может показывать проекцию координат.
class WaterMolecularDynamics {
public:
    WaterMolecularDynamics();

    void setTargetTemperature(double temperatureK);
    void setTargetPressure(double pressureMPa);
    void advance(unsigned int integrationSteps);

    const std::vector<WaterMolecule>& molecules() const;
    double boxLengthA() const;
    const Measurements& measurements() const;

private:
    std::vector<WaterMolecule> molecules_;
    double boxLengthA_ = 0.0;
    double targetTemperatureK_ = 300.0;
    double targetPressureMPa_ = 0.1;
    double timeStepFs_ = 0.25;
    Measurements measurements_;

    void initializeWater();
    void calculateForces(double& virialKJperMol);
    void calculateBondAndAngleForces(double& virialKJperMol);
    void calculateIntermolecularForces(double& virialKJperMol);
    void integrateStep();
    void applyThermostat();
    void applyBarostat(double virialKJperMol);
    void measure(double virialKJperMol);
    void wrapIntoBox(Vec3& position) const;
    Vec3 minimumImage(Vec3 displacement) const;
};

} // namespace watermd
