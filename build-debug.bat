@echo off
setlocal
cd /d "%~dp0"

call "%~dp0_configure.bat"
if errorlevel 1 exit /b 1

echo Building Debug...
cmake --build build --config Debug --target AIVoiceTranslator
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

set "OUT=%~dp0bin\Debug"
if not exist "%OUT%" mkdir "%OUT%"
copy /Y "build\AIVoiceTranslator_artefacts\Debug\AI Voice Translator.exe" "%OUT%\AIVoiceTranslator.exe" >nul
if errorlevel 1 (
    echo Build succeeded but the exe was not where we expected.
    pause
    exit /b 1
)
copy /Y "build\AIVoiceTranslator_artefacts\Debug\*.dll" "%OUT%\" >nul 2>nul
copy /Y "build\AIVoiceTranslator_artefacts\Debug\*.pdb" "%OUT%\" >nul 2>nul

echo.
echo Debug is in bin\Debug\
echo This is for you. Do not send this folder to the client.
start "" "%OUT%\AIVoiceTranslator.exe"
