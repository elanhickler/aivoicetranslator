@echo off
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
    echo CMake is not on PATH.
    echo Install CMake and Visual Studio 2022 with "Desktop development with C++".
    pause
    exit /b 1
)

if exist build\CMakeCache.txt (
    findstr /C:"CMAKE_HOME_DIRECTORY" build\CMakeCache.txt | findstr /I /C:"%CD:\=/%" >nul
    if errorlevel 1 (
        echo CMake cache is from an old folder. Clearing build\ ...
        rmdir /s /q build
    )
)

echo Configuring...
cmake -B build -A x64
if errorlevel 1 (
    echo Configure failed. Clearing build\ and retrying...
    if exist build rmdir /s /q build
    cmake -B build -A x64
    if errorlevel 1 (
        echo Configure failed.
        pause
        exit /b 1
    )
)
exit /b 0
