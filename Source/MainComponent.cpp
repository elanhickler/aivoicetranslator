#include "MainComponent.h"

MainComponent::MainComponent()
{
    juce::LookAndFeel::getDefaultLookAndFeel().setColour (
        juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1e1e1e));

    engine.setListener (this);

    auto err = deviceManager.initialise (1, 0, nullptr, true);
    if (err.isNotEmpty())
        statusLabel.setText (err, juce::dontSendNotification);

   #if JUCE_WINDOWS
    deviceManager.setCurrentAudioDeviceType ("Windows Audio", true);
   #endif
    preferCableInput();
    deviceManager.addAudioCallback (&engine);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager, 1, 2, 0, 0, false, false, false, false);
    deviceViewport.setViewedComponent (deviceSelector.get(), false);
    deviceViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (deviceViewport);

    listenButton.setClickingTogglesState (true);
    listenButton.onClick = [this]
    {
        engine.setApiUrl (urlEditor.getText().trim());
        const bool on = listenButton.getToggleState();
        engine.setListening (on);
        listenButton.setButtonText (on ? "Listening" : "Listen");
        statusLabel.setText (on ? "Listening" : "Idle", juce::dontSendNotification);
    };
    addAndMakeVisible (listenButton);

    statusLabel.setText ("Idle", juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    urlLabel.attachToComponent (&urlEditor, true);
    addAndMakeVisible (urlLabel);
    urlEditor.setText ("http://127.0.0.1:5000/v1/chat/completions");
    urlEditor.setEscapeAndReturnKeysConsumed (false);
    addAndMakeVisible (urlEditor);

    hintLabel.setText ("Speak after Listen. Pause to send.", juce::dontSendNotification);
    hintLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (hintLabel);

    log.setMultiLine (true);
    log.setReadOnly (true);
    log.setCaretVisible (false);
    log.setFont (juce::FontOptions (15.0f));
    addAndMakeVisible (log);

    setSize (720, 640);
}

MainComponent::~MainComponent()
{
    engine.setListening (false);
    deviceManager.removeAudioCallback (&engine);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (32);
    listenButton.setBounds (top.removeFromLeft (140));
    top.removeFromLeft (10);
    statusLabel.setBounds (top);

    r.removeFromTop (12);
    auto urlRow = r.removeFromTop (28);
    urlRow.removeFromLeft (80);
    urlEditor.setBounds (urlRow);

    r.removeFromTop (12);
    const int selH = juce::jlimit (180, 260, r.getHeight() / 3);
    deviceViewport.setBounds (r.removeFromTop (selH));
    deviceSelector->setSize (deviceViewport.getMaximumVisibleWidth(),
                             juce::jmax (deviceSelector->getHeight(), selH));

    r.removeFromTop (8);
    hintLabel.setBounds (r.removeFromTop (22));
    log.setBounds (r);
}

void MainComponent::engineStatus (const juce::String& s)
{
    statusLabel.setText (s, juce::dontSendNotification);
}

void MainComponent::engineHeard (const juce::String& userText)
{
    appendLog ("You: " + userText);
}

void MainComponent::engineReplied (const juce::String& aiText)
{
    appendLog ("AI: " + aiText + "\n");
}

void MainComponent::appendLog (const juce::String& line)
{
    log.moveCaretToEnd();
    log.insertTextAtCaret (line + "\n");
    log.moveCaretToEnd();
}

void MainComponent::preferCableInput()
{
    auto* type = deviceManager.getCurrentDeviceTypeObject();
    if (type == nullptr)
        return;

    const auto names = type->getDeviceNames (true);
    int cable = -1;
    for (int i = 0; i < names.size(); ++i)
        if (names[i].containsIgnoreCase ("CABLE Output"))
            cable = i;
    if (cable < 0)
        return;

    auto setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = names[cable];
    setup.useDefaultInputChannels = true;
    setup.outputDeviceName.clear();
    deviceManager.setAudioDeviceSetup (setup, true);
}
