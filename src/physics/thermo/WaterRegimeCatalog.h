#pragma once

#include <array>

// Справочная карточка физического режима воды или вещества её состава.
// Описание нужно для интерфейса и не подменяет физический расчёт.
struct WaterRegimeInfo {
    const wchar_t* name;
    const wchar_t* behavior;
    const wchar_t* conditions;
    const wchar_t* modelNote;
};

// Хранит краткие пояснения к девяти режимам, которые пользователь
// может исследовать в проекте. Реальный расчёт для каждого режима
// требует соответствующей физической модели.
class WaterRegimeCatalog {
public:
    static const std::array<WaterRegimeInfo, 9>& all();
};
