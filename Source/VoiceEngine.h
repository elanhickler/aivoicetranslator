#pragma once

#include <JuceHeader.h>
#include <vector>

struct whisper_context;

class VoiceEngine final : public juce::AudioIODeviceCallback,
                          private juce::Thread
{
public:
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void engineStatus (const juce::String&) = 0;
        virtual void engineHeard (const juce::String& userText) = 0;
        virtual void engineReplied (const juce::String& aiText) = 0;
    };

    VoiceEngine();
    ~VoiceEngine() override;

    void setListener (Listener* l) { listener = l; }
    void setApiUrl (juce::String url) { apiUrl = std::move (url); }
    void setListening (bool shouldListen);
    bool isListening() const { return listening.load(); }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    void run() override;
    bool ensureModel();
    juce::String transcribe (const std::vector<float>& audio16k);
    juce::String askOoba (const juce::String& userText);
    void speakReply (const juce::String& text);
    void postStatus (juce::String s);
    static std::vector<float> resampleTo16k (const float* in, int n, double srcRate);
    static std::vector<float> resample (const float* in, int n, double srcRate, double dstRate);
    static juce::String stripForSpeech (juce::String text);

    Listener* listener = nullptr;
    juce::String apiUrl { "http://127.0.0.1:5000/v1/chat/completions" };
    juce::String systemPrompt { "You are a helpful assistant. Keep replies short." };

    std::atomic<bool> listening { false };
    std::atomic<bool> busy { false };
    double sampleRate = 48000.0;

    juce::CriticalSection lock;
    std::vector<float> capture;
    std::vector<float> pending;
    double pendingRate = 16000.0;
    juce::WaitableEvent workReady;

    std::vector<float> playback;
    int playbackPos = 0;
    juce::WaitableEvent playbackDone;

    bool speaking = false;
    int loudBlocks = 0;
    int quietBlocks = 0;
    int windowSamples = 1440;
    int windowFilled = 0;
    double windowEnergy = 0.0;

    whisper_context* ctx = nullptr;
    juce::Array<juce::var> history;

    static constexpr float kSpeechRms = 0.015f;
    static constexpr int kStartBlocks = 3;
    static constexpr int kSilenceMs = 800;
    static constexpr int kMinSpeechMs = 350;
    static constexpr int kBlockMs = 30;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoiceEngine)
};
