#include "VoiceEngine.h"

#include <whisper.h>
#include <cmath>
#include <cstdint>

#if JUCE_WINDOWS
 #include <sapi.h>
#endif

namespace
{
constexpr juce::int64 kMinModelBytes = 10 * 1024 * 1024;

juce::File modelBesideExe()
{
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile ("models")
        .getChildFile ("ggml-base.en.bin");
}

juce::File modelInAppData()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("AIVoiceTranslator")
        .getChildFile ("ggml-base.en.bin");
}

bool isUsableModel (const juce::File& f)
{
    return f.existsAsFile() && f.getSize() > kMinModelBytes;
}

juce::File modelFile()
{
    if (isUsableModel (modelBesideExe()))
        return modelBesideExe();
    if (isUsableModel (modelInAppData()))
        return modelInAppData();
    return modelBesideExe();
}

bool downloadModelTo (const juce::File& dest)
{
    dest.getParentDirectory().createDirectory();
    dest.deleteFile();

    juce::URL url ("https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin");
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs (300000)
                    .withNumRedirectsToFollow (8);
    std::unique_ptr<juce::InputStream> in (url.createInputStream (opts));
    if (in == nullptr)
        return false;

    juce::int64 written = 0;
    {
        juce::FileOutputStream out (dest);
        if (out.failedToOpen())
            return false;
        written = out.writeFromInputStream (*in, -1);
        out.flush();
    }

    if (written < kMinModelBytes)
    {
        dest.deleteFile();
        return false;
    }

    return true;
}

juce::var makeMsg (const juce::String& role, const juce::String& content)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("role", role);
    o->setProperty ("content", content);
    return juce::var (o);
}
} // namespace

VoiceEngine::VoiceEngine() : juce::Thread ("voice-worker")
{
    capture.reserve (48000 * 60);
    startThread();
}

VoiceEngine::~VoiceEngine()
{
    listening = false;
    signalThreadShouldExit();
    workReady.signal();
    playbackDone.signal();
    stopThread (8000);
    if (ctx != nullptr)
        whisper_free (ctx);
}

void VoiceEngine::setListening (bool shouldListen)
{
    listening = shouldListen;
    if (! shouldListen)
    {
        const juce::ScopedLock sl (lock);
        capture.clear();
        speaking = false;
        loudBlocks = 0;
        quietBlocks = 0;
        windowFilled = 0;
        windowEnergy = 0.0;
    }
}

void VoiceEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 48000.0;
    windowSamples = juce::jmax (1, (int) std::lround (sampleRate * kBlockMs / 1000.0));
    const juce::ScopedLock sl (lock);
    capture.clear();
    speaking = false;
    loudBlocks = 0;
    quietBlocks = 0;
    windowFilled = 0;
    windowEnergy = 0.0;
    playback.clear();
    playbackPos = 0;
}

void VoiceEngine::audioDeviceStopped() {}

void VoiceEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                    int numInputChannels,
                                                    float* const* outputChannelData,
                                                    int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    bool finishedPlayback = false;
    {
        const juce::ScopedLock sl (lock);
        for (int i = 0; i < numSamples; ++i)
        {
            float s = 0.0f;
            if (playbackPos < (int) playback.size())
                s = playback[(size_t) playbackPos++];
            if (outputChannelData != nullptr)
            {
                for (int ch = 0; ch < numOutputChannels; ++ch)
                    if (outputChannelData[ch] != nullptr)
                        outputChannelData[ch][i] = s;
            }
        }
        if (! playback.empty() && playbackPos >= (int) playback.size())
        {
            playback.clear();
            playbackPos = 0;
            finishedPlayback = true;
        }
    }
    if (finishedPlayback)
        playbackDone.signal();

    if (! listening.load() || busy.load()
        || numInputChannels < 1 || inputChannelData == nullptr || inputChannelData[0] == nullptr)
        return;

    const float* left = inputChannelData[0];
    const float* right = (numInputChannels > 1 ? inputChannelData[1] : nullptr);
    const int silenceBlocks = juce::jmax (1, kSilenceMs / kBlockMs);
    const int minBlocks = juce::jmax (1, kMinSpeechMs / kBlockMs);

    for (int i = 0; i < numSamples; ++i)
    {
        float s = left[i];
        if (right != nullptr)
            s = 0.5f * (s + right[i]);

        if (speaking)
            capture.push_back (s);

        windowEnergy += (double) s * (double) s;
        ++windowFilled;

        if (windowFilled < windowSamples)
            continue;

        const float rms = (float) std::sqrt (windowEnergy / (double) windowSamples);
        windowEnergy = 0.0;
        windowFilled = 0;

        if (! speaking)
        {
            loudBlocks = rms >= kSpeechRms ? loudBlocks + 1 : 0;
            if (loudBlocks >= kStartBlocks)
            {
                speaking = true;
                quietBlocks = 0;
                capture.assign ((size_t) windowSamples, 0.0f);
            }
        }
        else
        {
            if (rms < kSpeechRms)
                ++quietBlocks;
            else
                quietBlocks = 0;

            if (quietBlocks >= silenceBlocks && (int) capture.size() >= minBlocks * windowSamples)
            {
                const juce::ScopedLock sl (lock);
                pending.swap (capture);
                pendingRate = sampleRate;
                capture.clear();
                speaking = false;
                loudBlocks = 0;
                quietBlocks = 0;
                busy = true;
                workReady.signal();
            }
        }
    }
}

void VoiceEngine::run()
{
   #if JUCE_WINDOWS
    CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
   #endif

    if (! ensureModel())
    {
       #if JUCE_WINDOWS
        CoUninitialize();
       #endif
        return;
    }

    postStatus ("Loading Whisper...");
    ctx = whisper_init_from_file_with_params (modelFile().getFullPathName().toRawUTF8(),
                                              whisper_context_default_params());
    if (ctx == nullptr)
    {
        postStatus ("Failed to load Whisper model");
       #if JUCE_WINDOWS
        CoUninitialize();
       #endif
        return;
    }

    postStatus ("Ready");

    while (! threadShouldExit())
    {
        workReady.wait (-1);
        if (threadShouldExit())
            break;

        std::vector<float> deviceAudio;
        double rate = 16000.0;
        {
            const juce::ScopedLock sl (lock);
            deviceAudio.swap (pending);
            rate = pendingRate;
        }

        if (deviceAudio.empty())
        {
            busy = false;
            continue;
        }

        postStatus ("Transcribing...");
        auto audio16k = resampleTo16k (deviceAudio.data(), (int) deviceAudio.size(), rate);
        auto text = transcribe (audio16k).trim();

        if (text.isEmpty())
        {
            postStatus (listening.load() ? "Listening" : "Idle");
            busy = false;
            continue;
        }

        if (listener != nullptr)
        {
            auto* l = listener;
            juce::MessageManager::callAsync ([l, text] { l->engineHeard (text); });
        }

        postStatus ("Waiting for ooba...");
        auto reply = askOoba (text);

        if (listener != nullptr)
        {
            auto* l = listener;
            juce::MessageManager::callAsync ([l, reply] { l->engineReplied (reply); });
        }

        if (! reply.startsWithIgnoreCase ("(ooba") && reply.isNotEmpty())
        {
            postStatus ("Speaking...");
            speakReply (reply);
        }

        busy = false;
        postStatus (listening.load() ? "Listening" : "Idle");
    }

   #if JUCE_WINDOWS
    CoUninitialize();
   #endif
}

bool VoiceEngine::ensureModel()
{
    if (isUsableModel (modelFile()))
        return true;

    postStatus ("Downloading Whisper model (first run, ~140MB)...");

    if (downloadModelTo (modelBesideExe()) || downloadModelTo (modelInAppData()))
        return isUsableModel (modelFile());

    postStatus ("Could not save Whisper model (not ooba). Need internet, or put ggml-base.en.bin in models/ next to the exe.");
    return false;
}

juce::String VoiceEngine::transcribe (const std::vector<float>& audio16k)
{
    if (ctx == nullptr || audio16k.empty())
        return {};

    auto params = whisper_full_default_params (WHISPER_SAMPLING_GREEDY);
    params.print_progress = false;
    params.print_special = false;
    params.print_realtime = false;
    params.print_timestamps = false;
    params.language = "en";
    params.n_threads = juce::jmax (1, (int) juce::SystemStats::getNumCpus() / 2);

    if (whisper_full (ctx, params, audio16k.data(), (int) audio16k.size()) != 0)
        return {};

    juce::String text;
    const int n = whisper_full_n_segments (ctx);
    for (int i = 0; i < n; ++i)
        text += juce::String (juce::CharPointer_UTF8 (whisper_full_get_segment_text (ctx, i)));
    return text;
}

juce::String VoiceEngine::askOoba (const juce::String& userText)
{
    if (history.isEmpty())
        history.add (makeMsg ("system", systemPrompt));
    history.add (makeMsg ("user", userText));

    auto* root = new juce::DynamicObject();
    root->setProperty ("messages", history);
    root->setProperty ("max_tokens", 512);
    root->setProperty ("mode", "chat-instruct");
    const auto body = juce::JSON::toString (juce::var (root));

    juce::URL url (apiUrl);
    auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                    .withExtraHeaders ("Content-Type: application/json\r\nAccept: application/json\r\n")
                    .withConnectionTimeoutMs (300000)
                    .withHttpRequestCmd ("POST");

    std::unique_ptr<juce::InputStream> in (url.withPOSTData (body).createInputStream (opts));
    if (in == nullptr)
        return "(ooba not reachable)";

    const auto parsed = juce::JSON::parse (in->readEntireStreamAsString());
    auto reply = parsed.getProperty ("choices", {})[0]
                     .getProperty ("message", {})
                     .getProperty ("content", juce::var ("(empty reply)"))
                     .toString()
                     .trim();

    history.add (makeMsg ("assistant", reply));
    return reply;
}

void VoiceEngine::postStatus (juce::String s)
{
    if (listener == nullptr)
        return;
    auto* l = listener;
    juce::MessageManager::callAsync ([l, s] { l->engineStatus (s); });
}

std::vector<float> VoiceEngine::resampleTo16k (const float* in, int n, double srcRate)
{
    return resample (in, n, srcRate, 16000.0);
}

std::vector<float> VoiceEngine::resample (const float* in, int n, double srcRate, double dstRate)
{
    if (n <= 0)
        return {};
    if (std::abs (srcRate - dstRate) < 1.0)
        return { in, in + n };

    const double ratio = srcRate / dstRate;
    const int outN = juce::jmax (1, (int) std::floor ((double) n / ratio));
    std::vector<float> out ((size_t) outN);
    juce::LagrangeInterpolator interp;
    interp.reset();
    interp.process (ratio, in, out.data(), outN);
    return out;
}

juce::String VoiceEngine::stripForSpeech (juce::String text)
{
    while (true)
    {
        const int start = text.indexOfIgnoreCase ("<think>");
        if (start < 0)
            break;
        int end = text.indexOfIgnoreCase ("</think>");
        if (end < 0)
        {
            text = text.substring (0, start);
            break;
        }
        text = text.substring (0, start) + text.substring (end + 8);
    }
    return text.trim();
}

void VoiceEngine::speakReply (const juce::String& text)
{
    const auto spoken = stripForSpeech (text);
    if (spoken.isEmpty())
        return;

    std::vector<float> pcm16k;

   #if JUCE_WINDOWS
    ISpVoice* voice = nullptr;
    if (FAILED (CoCreateInstance (CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**) &voice)) || voice == nullptr)
        return;

    IStream* mem = nullptr;
    if (FAILED (CreateStreamOnHGlobal (nullptr, TRUE, &mem)) || mem == nullptr)
    {
        voice->Release();
        return;
    }

    ISpStream* spStream = nullptr;
    if (FAILED (CoCreateInstance (CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, (void**) &spStream)) || spStream == nullptr)
    {
        mem->Release();
        voice->Release();
        return;
    }

    WAVEFORMATEX fmt {};
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = 16000;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = 32000;

    if (FAILED (spStream->SetBaseStream (mem, SPDFID_WaveFormatEx, &fmt)))
    {
        spStream->Release();
        mem->Release();
        voice->Release();
        return;
    }

    voice->SetOutput (spStream, TRUE);
    voice->Speak (spoken.toWideCharPointer(), SPF_DEFAULT | SPF_PURGEBEFORESPEAK, nullptr);

    STATSTG st {};
    mem->Stat (&st, STATFLAG_NONAME);
    LARGE_INTEGER zero {};
    mem->Seek (zero, STREAM_SEEK_SET, nullptr);

    const ULONG bytes = st.cbSize.LowPart;
    std::vector<char> raw ((size_t) bytes);
    ULONG got = 0;
    mem->Read (raw.data(), bytes, &got);

    const int n = (int) (got / sizeof (int16_t));
    pcm16k.resize ((size_t) n);
    const auto* s16 = reinterpret_cast<const int16_t*> (raw.data());
    for (int i = 0; i < n; ++i)
        pcm16k[(size_t) i] = (float) s16[i] / 32768.0f;

    spStream->Release();
    mem->Release();
    voice->Release();
   #endif

    if (pcm16k.empty())
        return;

    auto devicePcm = resample (pcm16k.data(), (int) pcm16k.size(), 16000.0, sampleRate);
    {
        const juce::ScopedLock sl (lock);
        playback = std::move (devicePcm);
        playbackPos = 0;
    }

    while (! threadShouldExit())
    {
        bool done = false;
        {
            const juce::ScopedLock sl (lock);
            done = playback.empty();
        }
        if (done)
            break;
        playbackDone.wait (100);
    }
}
