#include "physics/SimulationEngine.h"
#include <algorithm>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// Запускает то же ядро без Windows и графики. Аргументы: эксперимент 1..7,
// число шагов. Нужен для воспроизводимых расчётов и проверки физики.
int main(int argc, char** argv) {
    try {
        auto parse = [](const char* value) {
            std::size_t end = 0;
            const std::string input(value);
            const int result = std::stoi(input, &end);
            if (end != input.size()) throw std::invalid_argument("Expected an integer argument");
            return result;
        };
        const int experiment = argc > 1 ? parse(argv[1]) : 1;
        const int steps = argc > 2 ? parse(argv[2]) : 1000;
        if (argc > 3 || experiment < 1 || experiment > 7 || steps < 0 || steps > 10000000)
            throw std::invalid_argument("Usage: physics_headless [1..7] [0..10000000 steps]");
        physics::SimulationEngine engine;
        engine.reset(static_cast<physics::Experiment>(experiment - 1));
        for (int remaining = steps; remaining > 0;) {
            const int batch = std::min(32, remaining);
            engine.advance(static_cast<unsigned int>(batch));
            remaining -= batch;
        }
        const auto& report = engine.report();
        std::cout << "time_fs,temperature_K,pressure_MPa,density_kg_m3,kinetic_kJ_mol,potential_kJ_mol,theta_e\n"
            << std::setprecision(12) << report.timeFs << ',' << report.temperatureK << ',' << report.pressureMPa << ','
            << report.densityKgM3 << ',' << report.kineticEnergy << ',' << report.potentialEnergy << ','
            << report.field.electronTemperatureRatio << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
