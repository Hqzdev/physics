#include "StateAppearance.h"

StateAppearance StateAppearance::forState(physics::LocalState state) {
    using physics::LocalState;
    switch (state) {
    case LocalState::IceLike: return {109, 244, 255, L"Льдоподобная структура"};
    case LocalState::LiquidLike: return {47, 133, 255, L"Жидкоподобная структура"};
    case LocalState::VaporLike: return {255, 217, 74, L"Газоподобное окружение"};
    case LocalState::Interface: return {83, 224, 142, L"Граница / мало соседей"};
    case LocalState::HotFluid: return {255, 153, 64, L"Горячий флюид (кандидат)"};
    case LocalState::Plasma: return {205, 104, 255, L"Ионизированная смесь"};
    case LocalState::DegeneratePlasma: return {255, 87, 117, L"Вырожденные электроны"};
    case LocalState::Unknown: return {155, 170, 190, L"Накопление наблюдений"};
    }
    return {155, 170, 190, L"Накопление наблюдений"};
}
