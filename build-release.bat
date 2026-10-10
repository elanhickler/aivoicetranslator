@echo off
setlocal
cd /d "%~dp0"

call "%~dp0_configure.bat"
if errorlevel 1 exit /b 1

echo Building Release...
cmake --build build --config Release --target AIVoiceTranslator
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)

set "OUT=%~dp0bin\Release"
if not exist "%OUT%" mkdir "%OUT%"
copy /Y "build\AIVoiceTranslator_artefacts\Release\AI Voice Translator.exe" "%OUT%\AIVoiceTranslator.exe" >nul
if errorlevel 1 (
    echo Build succeeded but the exe was not where we expected.
    pause
    exit /b 1
)
copy /Y "build\AIVoiceTranslator_artefacts\Release\*.dll" "%OUT%\" >nul 2>nul
copy /Y "build\AIVoiceTranslator_artefacts\Release\*.pdb" "%OUT%\" >nul 2>nul
copy /Y "build\AIVoiceTranslator_artefacts\Release\vc_redist.x64.exe" "%OUT%\" >nul 2>nul

echo.
echo Release is in bin\Release\
echo Zip that whole folder and send it to the client. Do not send bin\Debug.
echo.
pause
