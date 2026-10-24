@echo off
echo [INFO] Настройка Visual Studio...

:: Путь к Visual Studio 2026 (версия 18)
set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"

if not exist "%VCVARS%" (
    echo [ERROR] Visual Studio не найдена: %VCVARS%
    pause
    exit /b 1
)

:: Настраиваем окружение компилятора
call "%VCVARS%" >nul 2>&1

echo [INFO] Компиляция молекулярной динамики воды...
cl /EHsc /O2 /std:c++17 /utf-8 src\main.cpp src\physics\md\WaterMolecularDynamics.cpp src\app\Application.cpp user32.lib gdi32.lib /Fe:sim.exe

if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Сборка завершена. Запуск sim.exe...
    sim.exe
) else (
    echo [ERROR] Ошибка сборки.
    pause
)
