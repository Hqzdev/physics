@echo off
chcp 65001 >nul
setlocal
pushd "%~dp0"
where cl >nul 2>&1
if not errorlevel 1 goto compile

echo [INFO] Настройка Visual Studio...
set "PHYSICS_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PHYSICS_VSWHERE%" goto missing_compiler
for /f "usebackq delims=" %%i in (`"%PHYSICS_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PHYSICS_VS=%%i"
if not defined PHYSICS_VS goto missing_compiler
call "%PHYSICS_VS%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 goto failed

:compile
if not exist build\win mkdir build\win
echo [INFO] Компиляция физического движка...
cl /nologo /EHsc /O2 /W4 /std:c++17 /utf-8 /permissive- /DNOMINMAX /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    src\main.cpp src\app\Application.cpp src\app\SimulationRenderer.cpp src\app\StateAppearance.cpp ^
    src\app\BufferedPaint.cpp ^
    src\physics\SimulationEngine.cpp src\physics\core\ParticleSystem.cpp ^
    src\physics\core\LangevinIntegrator.cpp src\physics\core\VolumeController.cpp ^
    src\physics\models\WaterForceField.cpp src\physics\models\InitialConditions.cpp ^
    src\physics\models\EwaldElectrostatics.cpp ^
    src\physics\models\ElectronGas.cpp src\physics\models\PlasmaForceField.cpp ^
    src\physics\analysis\StructureAnalyzer.cpp user32.lib gdi32.lib /Fo:build\win\ /Fe:build\win\sim.exe
if errorlevel 1 goto failed
echo [SUCCESS] Запуск симуляции...
build\win\sim.exe
set "PHYSICS_EXIT=%ERRORLEVEL%"
popd
exit /b %PHYSICS_EXIT%

:missing_compiler
echo [ERROR] Установите Visual Studio с компонентом разработки классических приложений на C++.
goto failed

:failed
echo [ERROR] Сборка не выполнена.
popd
pause
exit /b 1
