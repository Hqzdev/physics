cmake_minimum_required(VERSION 3.16)
project(WaterPhysics LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(physics_core
    src/physics/core/ParticleSystem.cpp
    src/physics/core/LangevinIntegrator.cpp
    src/physics/core/VolumeController.cpp
    src/physics/models/WaterForceField.cpp
    src/physics/models/EwaldElectrostatics.cpp
    src/physics/models/InitialConditions.cpp
    src/physics/models/ElectronGas.cpp
    src/physics/models/PlasmaForceField.cpp
    src/physics/analysis/StructureAnalyzer.cpp
    src/physics/SimulationEngine.cpp)
target_include_directories(physics_core PUBLIC src)

if(MSVC)
    target_compile_options(physics_core PRIVATE /W4 /utf-8 /permissive-)
else()
    target_compile_options(physics_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(physics_headless src/headless.cpp)
target_link_libraries(physics_headless PRIVATE physics_core)
if(MSVC)
    target_compile_options(physics_headless PRIVATE /W4 /utf-8 /permissive-)
endif()

if(WIN32)
    add_executable(sim src/main.cpp src/app/Application.cpp src/app/BufferedPaint.cpp
        src/app/SimulationRenderer.cpp src/app/StateAppearance.cpp)
    target_compile_definitions(sim PRIVATE NOMINMAX UNICODE _UNICODE WIN32_LEAN_AND_MEAN)
    target_link_libraries(sim PRIVATE physics_core user32 gdi32)
    if(MSVC)
        target_compile_options(sim PRIVATE /W4 /utf-8 /permissive-)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(physics_tests tests/PhysicsTests.cpp src/app/StateAppearance.cpp)
    target_link_libraries(physics_tests PRIVATE physics_core)
    if(MSVC)
        target_compile_options(physics_tests PRIVATE /W4 /utf-8 /permissive-)
    endif()
    add_test(NAME physics_invariants COMMAND physics_tests)
endif()
