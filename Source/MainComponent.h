#pragma once

#include "VoiceEngine.h"

class MainComponent final : public juce::Component,
                            private VoiceEngine::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void engineStatus (const juce::String&) override;
    void engineHeard (const juce::String& userText) override;
    void engineReplied (const juce::String& aiText) override;
    void appendLog (const juce::String& line);
    void preferCableInput();

    juce::AudioDeviceManager deviceManager;
    VoiceEngine engine;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::Viewport deviceViewport;

    juce::TextButton muteButton { "Mute" };
    juce::Label statusLabel;
    juce::Label urlLabel { {}, "Ooba API" };
    juce::TextEditor urlEditor;
    juce::TextEditor log;

    void applyMute (bool muted);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
