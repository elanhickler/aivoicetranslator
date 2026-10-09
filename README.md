# AI Voice Translator

Talk to oobabooga without a record button. Listens to Virtual Cable or a mic, transcribes, sends the turn to ooba.

## Build

Need Git, CMake, and Visual Studio 2022 with **Desktop development with C++**.

1. Clone this repo.
2. Double-click `build.bat`.
3. First run downloads dependencies and compiles. Later runs just rebuild.
4. You get `bin\AIVoiceTranslator.exe`, and it launches.

## Use

1. Start oobabooga with `--api`.
2. Pick **CABLE Output** (or your mic) in the app.
3. Press **Listen**, talk, pause.
4. First Listen may download `ggml-base.en.bin` (~140MB) next to the exe.

Default API: `http://127.0.0.1:5000/v1/chat/completions`
