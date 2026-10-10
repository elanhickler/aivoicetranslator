#pragma once

#include "VoiceEngine.h"

class MainComponent final : public juce::Component,
                            private juce::Timer,
                            private VoiceEngine::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void engineStatus (const juce::String&) override;
    void engineHeard (const juce::String& userText) override;
    void engineReplied (const juce::String& aiText) override;
    void appendLog (const juce::String& line);
    void preferCableInput();
    void loadSettings();
    void saveSettings();
    void browseTextGen();
    void startTextGen();
    void guessTextGen();
    void checkApi();
    juce::File settingsFile() const;
    juce::URL apiProbeUrl() const;

    juce::AudioDeviceManager deviceManager;
    VoiceEngine engine;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
    juce::Viewport deviceViewport;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::TextButton muteButton { "Mute" };
    juce::Label statusLabel;
    juce::Label apiStateLabel { {}, "API ?" };
    juce::Label textgenLabel { {}, "TextGen" };
    juce::TextEditor textgenEditor;
    juce::TextButton browseButton { "Browse" };
    juce::TextButton startButton { "Start" };
    juce::Label urlLabel { {}, "API" };
    juce::TextEditor urlEditor;
    juce::TextEditor log;

    bool apiUp = false;
    bool muted = false;

    void applyMute (bool shouldMute);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
