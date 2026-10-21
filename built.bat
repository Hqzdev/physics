@echo off
echo [INFO] Setting up Visual Studio environment...

:: Путь для Visual Studio 2026 (версия 18)
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo [ERROR] Visual Studio not found at: %VCVARS%
    pause
    exit /b 1
)

:: Подключаем компилятор
call "%VCVARS%" >nul 2>&1

echo [INFO] Compiling CPU Simulator...
cl /EHsc /O2 /std:c++17 src\main.cpp src\physics\ParticleSystem.cpp src\render\Renderer.cpp src\app\Application.cpp user32.lib gdi32.lib /Fe:sim.exe

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Build complete! Running sim.exe...
    sim.exe
) else (
    echo [ERROR] Build failed.
    pause
)