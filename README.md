# AI Voice Translator

Talk to oobabooga without a record button. Listens to Virtual Cable or a mic, transcribes, sends the turn to ooba.

## Build

Need Git, CMake, and Visual Studio 2022 with **Desktop development with C++**.

1. Clone this repo.
2. Double-click `build.bat`.
3. First run downloads dependencies and compiles. Later runs just rebuild.
4. You get `bin\AIVoiceTranslator.exe`, and it launches.

## Start oobabooga (TextGen) with the API

Repo: https://github.com/oobabooga/textgen

The UI is on port **7860**. The API this app talks to is on port **5000**. `--api` turns that API on.

**One-click install** (folder with `start_windows.bat`):

```
start_windows.bat --api
```

To make it stick every launch, put this in `user_data/CMD_FLAGS.txt`:

```
--api
```

Then you can just double-click `start_windows.bat`.

**Portable desktop build** (unzip from [Releases](https://github.com/oobabooga/textgen/releases), run `textgen` / `textgen.bat`): portable already starts with `--api`. If chat works but this app cannot connect, add `--api` to `user_data/CMD_FLAGS.txt` and relaunch.

**From source:**

```
python server.py --api
```

Load a model in the UI (or `--model your-model.gguf` in `CMD_FLAGS.txt`). Check the API is up: http://127.0.0.1:5000/docs

If you used `--api-port 1234`, set this app’s Ooba API field to `http://127.0.0.1:1234/v1/chat/completions`.

## Use

1. Start oobabooga with `--api` as above, with a model loaded.
2. Pick **CABLE Output** (or your mic) in this app.
3. Talk. It is already listening. Mute if you need to.
4. First run may download `ggml-base.en.bin` (~140MB) next to the exe.

Default API: `http://127.0.0.1:5000/v1/chat/completions`
