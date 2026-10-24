#pragma once

namespace physics {

struct ElectronState {
    double pressurePa = 0.0;
    double freeEnergyJ = 0.0;
    double fermiTemperatureK = 0.0;
    double temperatureRatio = 0.0;
    double screeningLengthA = 0.0;
};

// Нерелятивистский идеальный газ электронов со спином 1/2.
// Число электронов определяет химический потенциал через распределение
// Ферми — Дирака. Из него следуют давление, свободная энергия и экранирование.
// Электроны являются полем, отдельные электронные точки не рисуются.
class ElectronGas {
public:
    ElectronState evaluate(double electronCount, double volumeA3, double temperatureK) const;
};

} // namespace physics
