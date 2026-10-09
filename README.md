# AI Voice Translator

JUCE app: pick a mic / Virtual Cable, press Listen, talk, pause, ooba replies. No Python, no `.bat`.

## On their computer

1. Start oobabooga with `--api`.
2. Send them `AIVoiceTranslator.exe` plus a `models` folder if the model is already downloaded.
3. Run the exe.
4. Confirm the input device is **CABLE Output** (or their mic).
5. Press **Listen** and talk. First run downloads `ggml-base.en.bin` (~140MB) next to the exe.

## Build here

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Exe: `build/AIVoiceTranslator_artefacts/Release/AI Voice Translator.exe`
