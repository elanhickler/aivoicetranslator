#include "MainComponent.h"
#include <cmath>

namespace
{
constexpr int kPad = 8;
constexpr int kGap = 6;
constexpr int kRow = 26;
constexpr int kLabelW = 56;
}

MainComponent::MainComponent() : juce::Thread ("api-ping")
{
    juce::LookAndFeel::getDefaultLookAndFeel().setColour (
        juce::ResizableWindow::backgroundColourId, juce::Colour (0xff1e1e1e));

    engine.setListener (this);

    auto err = deviceManager.initialise (1, 2, nullptr, true);
    if (err.isNotEmpty())
        statusLabel.setText (err, juce::dontSendNotification);

   #if JUCE_WINDOWS
    deviceManager.setCurrentAudioDeviceType ("Windows Audio", true);
   #endif
    preferCableInput();
    deviceManager.addAudioCallback (&engine);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent> (
        deviceManager, 1, 2, 1, 2, false, false, false, false);
    deviceViewport.setViewedComponent (deviceSelector.get(), false);
    deviceViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (deviceViewport);

    muteButton.setClickingTogglesState (true);
    muteButton.onClick = [this] { applyMute (muteButton.getToggleState()); };
    addAndMakeVisible (muteButton);

    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (statusLabel);

    apiStateLabel.setJustificationType (juce::Justification::centredRight);
    apiStateLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    addAndMakeVisible (apiStateLabel);

    textgenLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (textgenLabel);
    textgenEditor.setTextToShowWhenEmpty ("textgen.bat or textgen.exe", juce::Colours::grey);
    addAndMakeVisible (textgenEditor);
    browseButton.onClick = [this] { browseTextGen(); };
    addAndMakeVisible (browseButton);
    startButton.onClick = [this] { startTextGen(); };
    addAndMakeVisible (startButton);

    urlLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (urlLabel);
    urlEditor.setText ("http://127.0.0.1:5000/v1/chat/completions");
    urlEditor.setEscapeAndReturnKeysConsumed (false);
    urlEditor.onFocusLost = [this]
    {
        engine.setApiUrl (urlEditor.getText().trim());
        parseApiHostPort (probeHost, probePort);
        saveSettings();
        notify();
    };
    addAndMakeVisible (urlEditor);

    log.setMultiLine (true);
    log.setReadOnly (true);
    log.setCaretVisible (false);
    log.setScrollbarsShown (true);
    log.setFont (juce::FontOptions (15.0f));
    log.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141414));
    log.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff333333));
    log.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (0xff333333));
    addAndMakeVisible (log);

    loadSettings();
    parseApiHostPort (probeHost, probePort);
    if (textgenEditor.getText().trim().isEmpty())
        guessTextGen();

    applyMute (false);
    startThread();
    setSize (720, 620);
}

MainComponent::~MainComponent()
{
    signalThreadShouldExit();
    notify();
    stopThread (1500);
    saveSettings();
    engine.setListening (false);
    deviceManager.removeAudioCallback (&engine);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto r = getLocalBounds().reduced (kPad);

    auto top = r.removeFromTop (kRow);
    muteButton.setBounds (top.removeFromLeft (72));
    top.removeFromLeft (kGap);
    apiStateLabel.setBounds (top.removeFromRight (72));
    statusLabel.setBounds (top);

    r.removeFromTop (kGap);
    auto tg = r.removeFromTop (kRow);
    textgenLabel.setBounds (tg.removeFromLeft (kLabelW));
    startButton.setBounds (tg.removeFromRight (64));
    tg.removeFromRight (kGap);
    browseButton.setBounds (tg.removeFromRight (72));
    tg.removeFromRight (kGap);
    textgenEditor.setBounds (tg);

    r.removeFromTop (kGap);
    auto api = r.removeFromTop (kRow);
    urlLabel.setBounds (api.removeFromLeft (kLabelW));
    urlEditor.setBounds (api);

    r.removeFromTop (kGap);

    const int minLog = juce::jmax (160, r.getHeight() / 2);
    const int innerW = r.getWidth();
    if (std::abs (deviceSelector->getWidth() - innerW) > 1)
        deviceSelector->setSize (innerW, juce::jmax (1, deviceSelector->getHeight()));
    const int selH = juce::jmax (1, deviceSelector->getHeight());
    const int viewH = juce::jmin (selH, juce::jmax (80, r.getHeight() - minLog - kGap));
    deviceViewport.setBounds (r.removeFromTop (viewH));
    r.removeFromTop (kGap);
    log.setBounds (r);
}

void MainComponent::run()
{
    while (! threadShouldExit())
    {
        const bool up = probeApi();
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::MessageManager::callAsync ([safe, up]
        {
            if (safe != nullptr)
                safe->setApiState (up);
        });
        wait (4000);
    }
}

void MainComponent::applyMute (bool shouldMute)
{
    muted = shouldMute;
    engine.setApiUrl (urlEditor.getText().trim());
    engine.setListening (! shouldMute);
    muteButton.setToggleState (shouldMute, juce::dontSendNotification);
    muteButton.setButtonText (shouldMute ? "Unmute" : "Mute");
    if (shouldMute)
        statusLabel.setText ("Muted", juce::dontSendNotification);
    else if (statusLabel.getText() == "Muted")
        statusLabel.setText ("Listening", juce::dontSendNotification);
}

void MainComponent::engineStatus (const juce::String& s)
{
    if (! muted)
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
    setup.useDefaultOutputChannels = true;
    deviceManager.setAudioDeviceSetup (setup, true);
}

juce::File MainComponent::settingsFile() const
{
    return juce::File::getSpecialLocation (juce::File::currentExecutableFile)
        .getParentDirectory()
        .getChildFile ("settings.xml");
}

void MainComponent::loadSettings()
{
    auto xml = juce::XmlDocument::parse (settingsFile());
    if (xml == nullptr)
        return;

    const auto path = xml->getStringAttribute ("textgen");
    const auto api = xml->getStringAttribute ("api");
    if (path.isNotEmpty())
        textgenEditor.setText (path);
    if (api.isNotEmpty())
        urlEditor.setText (api);
}

void MainComponent::saveSettings()
{
    juce::XmlElement root ("settings");
    root.setAttribute ("textgen", textgenEditor.getText().trim());
    root.setAttribute ("api", urlEditor.getText().trim());
    root.writeTo (settingsFile());
}

void MainComponent::guessTextGen()
{
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int up = 0; up < 5; ++up)
    {
        for (const auto& child : dir.findChildFiles (juce::File::findDirectories, false, "*"))
        {
            if (! child.getFileName().containsIgnoreCase ("textgen"))
                continue;
            for (const auto& name : { "textgen.bat", "textgen.exe", "textgen.cmd" })
            {
                auto f = child.getChildFile (name);
                if (f.existsAsFile())
                {
                    textgenEditor.setText (f.getFullPathName());
                    saveSettings();
                    return;
                }
            }
        }
        dir = dir.getParentDirectory();
    }
}

void MainComponent::browseTextGen()
{
    auto start = juce::File (textgenEditor.getText().trim());
    if (! start.existsAsFile())
        start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> (
        "Locate textgen.bat or textgen.exe", start, "*.bat;*.cmd;*.exe");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& c)
                          {
                              auto f = c.getResult();
                              if (f.existsAsFile())
                              {
                                  textgenEditor.setText (f.getFullPathName());
                                  saveSettings();
                              }
                          });
}

void MainComponent::startTextGen()
{
    auto f = juce::File (textgenEditor.getText().trim());
    if (! f.existsAsFile())
    {
        browseTextGen();
        return;
    }

    saveSettings();
    const auto dir = f.getParentDirectory().getFullPathName();

   #if JUCE_WINDOWS
    juce::Process::openDocument ("cmd.exe",
                                 "/c start \"TextGen\" /D \"" + dir + "\" \"" + f.getFullPathName() + "\"");
   #else
    f.startAsProcess();
   #endif
}

void MainComponent::parseApiHostPort (juce::String& host, int& port) const
{
    juce::URL u (urlEditor.getText().trim());
    host = u.getDomain();
    if (host.isEmpty())
        host = "127.0.0.1";
    port = u.getPort();
    if (port <= 0)
        port = 5000;
}

bool MainComponent::probeApi() const
{
    juce::StreamingSocket sock;
    const bool up = sock.connect (probeHost, probePort, 200);
    if (up)
        sock.close();
    return up;
}

void MainComponent::setApiState (bool up)
{
    if (up == apiUp && apiStateLabel.getText() != "API ?")
        return;

    apiUp = up;
    apiStateLabel.setText (up ? "API up" : "API down", juce::dontSendNotification);
    apiStateLabel.setColour (juce::Label::textColourId,
                             up ? juce::Colours::lightgreen : juce::Colours::orange);

    if (! up && log.getText().isEmpty())
        log.setText ("API down. Browse to textgen.bat / textgen.exe, click Start, then Load a model in TextGen.\n");
}
