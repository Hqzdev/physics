#include "app/Application.h"
#include <exception>

// Точка входа: приложение — один объект, всё остальное он
// собирает сам в своём конструкторе.
int main() {
    try {
        Application app;
        return app.run();
    } catch (const std::exception& error) {
        MessageBoxA(nullptr, error.what(), "Simulation startup error", MB_OK | MB_ICONERROR);
        return 1;
    }
}
