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

    muteButton.setClickingTogglesState (true);
    muteButton.onClick = [this] { applyMute (muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    urlLabel.attachToComponent (&urlEditor, true);
    addAndMakeVisible (urlLabel);
    urlEditor.setText ("http://127.0.0.1:5000/v1/chat/completions");
    urlEditor.setEscapeAndReturnKeysConsumed (false);
    addAndMakeVisible (urlEditor);

    log.setMultiLine (true);
    log.setReadOnly (true);
    log.setCaretVisible (false);
    log.setScrollbarsShown (true);
    log.setFont (juce::FontOptions (15.0f));
    log.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141414));
    log.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    log.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (log);

    applyMute (false);
    setSize (720, 560);
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
    muteButton.setBounds (top.removeFromLeft (100));
    top.removeFromLeft (10);
    statusLabel.setBounds (top);

    r.removeFromTop (12);
    auto urlRow = r.removeFromTop (28);
    urlRow.removeFromLeft (80);
    urlEditor.setBounds (urlRow);

    r.removeFromTop (12);

    const int minLog = 80;
    const int gap = 6;
    deviceSelector->setSize (r.getWidth(), juce::jmax (1, deviceSelector->getHeight()));
    const int selH = juce::jmax (1, deviceSelector->getHeight());
    const int viewH = juce::jmin (selH, juce::jmax (0, r.getHeight() - minLog - gap));
    deviceViewport.setBounds (r.removeFromTop (viewH));
    deviceSelector->setSize (deviceViewport.getMaximumVisibleWidth(), selH);
    r.removeFromTop (gap);
    log.setBounds (r);
}

void MainComponent::applyMute (bool muted)
{
    engine.setApiUrl (urlEditor.getText().trim());
    engine.setListening (! muted);
    muteButton.setToggleState (muted, juce::dontSendNotification);
    muteButton.setButtonText (muted ? "Unmute" : "Mute");
    statusLabel.setText (muted ? "Muted" : "Listening", juce::dontSendNotification);
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
