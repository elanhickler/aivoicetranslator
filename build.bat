@echo off
setlocal
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
    echo CMake is not on PATH.
    echo Install CMake and Visual Studio 2022 with "Desktop development with C++".
    pause
    exit /b 1
)

echo Configuring...
cmake -B build -A x64
if errorlevel 1 (
    echo Configure failed.
    pause
    exit /b 1
)

echo Building Release...
cmake --build build --config Release --target AIVoiceTranslator
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

if not exist bin mkdir bin
copy /Y "build\AIVoiceTranslator_artefacts\Release\AI Voice Translator.exe" "bin\AIVoiceTranslator.exe" >nul
if errorlevel 1 (
    echo Build succeeded but the exe was not where we expected.
    pause
    exit /b 1
)
copy /Y "build\AIVoiceTranslator_artefacts\Release\*.dll" "bin\" >nul 2>nul
copy /Y "build\AIVoiceTranslator_artefacts\Release\*.pdb" "bin\" >nul 2>nul
copy /Y "build\AIVoiceTranslator_artefacts\Release\vc_redist.x64.exe" "bin\" >nul 2>nul

echo.
echo Built bin\AIVoiceTranslator.exe
start "" "%~dp0bin\AIVoiceTranslator.exe"
