#pragma once

#include "../physics/analysis/StructureAnalyzer.h"
#include <cstdint>

// Цвета принадлежат представлению. Физические модели не знают о палитре.
struct StateAppearance {
    std::uint8_t red, green, blue;
    const wchar_t* label;
    static StateAppearance forState(physics::LocalState state);
};
