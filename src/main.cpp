#include "app/Application.h"

// Точка входа: приложение — один объект, всё остальное он
// собирает сам в своём конструкторе.
int main() {
    Application app;
    return app.run();
}