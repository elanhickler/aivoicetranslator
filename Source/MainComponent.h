#pragma once

#include "VoiceEngine.h"

class MainComponent final : public juce::Component,
                            private juce::Thread,
                            private VoiceEngine::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void run() override;
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
    void setApiState (bool up);
    bool probeApi() const;
    juce::File settingsFile() const;
    void parseApiHostPort (juce::String& host, int& port) const;

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
    juce::String probeHost { "127.0.0.1" };
    int probePort = 5000;

    void applyMute (bool shouldMute);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
