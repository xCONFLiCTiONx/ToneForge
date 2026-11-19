#include "MainPanel.h"

MainPanel::MainPanel (ToneForgeAudio& engine, juce::AudioDeviceManager& devManager, juce::ApplicationProperties& appProps)
    : audio (engine), deviceManager (devManager), appProperties (appProps)
{
    setOpaque (true);
    setLookAndFeel (&lookAndFeel);

    // Title
    appTitle.setText ("TONEFORGE", juce::dontSendNotification);
    appTitle.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    appTitle.setColour (juce::Label::textColourId, juce::Colour (0xff8049e2));
    addAndMakeVisible (appTitle);

    // Presets
    presetLabel.setText ("PRESET:", juce::dontSendNotification);
    presetLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    presetLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa2a6b2));
    addAndMakeVisible (presetLabel);
    addAndMakeVisible (presetSelector);

    savePresetButton.setTooltip ("Save current settings as a new profile");
    savePresetButton.onClick = [this] { promptSaveProfile(); };
    addAndMakeVisible (savePresetButton);

    // Input Source (Critical for Focusrite Scarlett Solo!)
    inputSourceLabel.setText ("INPUT:", juce::dontSendNotification);
    inputSourceLabel.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    inputSourceLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa2a6b2));
    addAndMakeVisible (inputSourceLabel);

    inputSourceSelector.addItem ("Auto-Detect (Focusrite Solo)", 1);
    inputSourceSelector.addItem ("Ch 2: Instrument / Guitar", 2);
    inputSourceSelector.addItem ("Ch 1: Microphone (XLR)", 3);
    inputSourceSelector.addItem ("Stereo / Both (Ch 1+2)", 4);
    inputSourceSelector.setSelectedId (1, juce::dontSendNotification);
    inputSourceSelector.onChange = [this]
    {
        int id = inputSourceSelector.getSelectedId();
        if (id == 1)
            audio.inputSource.store (static_cast<int> (GuitarInputRouting::AutoDetect));
        else if (id == 2)
            audio.inputSource.store (static_cast<int> (GuitarInputRouting::GuitarCh2));
        else if (id == 3)
            audio.inputSource.store (static_cast<int> (GuitarInputRouting::MicCh1));
        else
            audio.inputSource.store (static_cast<int> (GuitarInputRouting::StereoSum));
    };
    addAndMakeVisible (inputSourceSelector);

    // Audio Setup Button
    audioSettingsButton.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    // Master Mute
    bool isMutedDefault = true;
    if (auto* userSettings = appProperties.getUserSettings())
        isMutedDefault = userSettings->getBoolValue ("masterMute", true);

    masterMuteButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffe63946));
    masterMuteButton.setToggleState (isMutedDefault, juce::dontSendNotification);
    audio.masterMute.store (isMutedDefault);
    masterMuteButton.onStateChange = [this]
    {
        bool muted = masterMuteButton.getToggleState();
        audio.masterMute.store (muted);
        if (auto* userSettings = appProperties.getUserSettings())
        {
            userSettings->setValue ("masterMute", muted);
            appProperties.saveIfNeeded();
        }
    };
    addAndMakeVisible (masterMuteButton);

    // Tuner HUD
    tunerNoteLabel.setText ("--", juce::dontSendNotification);
    tunerNoteLabel.setFont (juce::Font (juce::FontOptions (28.0f, juce::Font::bold)));
    tunerNoteLabel.setJustificationType (juce::Justification::centred);
    tunerNoteLabel.setColour (juce::Label::textColourId, juce::Colour (0xffffffff));
    addAndMakeVisible (tunerNoteLabel);

    tunerCentsLabel.setText ("CHROMATIC TUNER", juce::dontSendNotification);
    tunerCentsLabel.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    tunerCentsLabel.setJustificationType (juce::Justification::centred);
    tunerCentsLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa2a6b2));
    addAndMakeVisible (tunerCentsLabel);

    tunerFreqLabel.setText ("Play guitar to tune", juce::dontSendNotification);
    tunerFreqLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
    tunerFreqLabel.setJustificationType (juce::Justification::centred);
    tunerFreqLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7e8494));
    addAndMakeVisible (tunerFreqLabel);

    tunerMuteButton.onStateChange = [this] { audio.tunerMute.store (tunerMuteButton.getToggleState()); };
    addAndMakeVisible (tunerMuteButton);

    // 1. Noise Gate Module
    setupToggle (gateToggle, "NOISE GATE", false);
    gateToggle.onStateChange = [this] { audio.gateEnabled.store (gateToggle.getToggleState()); };
    setupKnob (gateThresholdSlider, "THRESH", -80.0, -20.0, -68.0, " dB");
    gateThresholdSlider.onValueChange = [this] { audio.gateThresholdDb.store (static_cast<float> (gateThresholdSlider.getValue())); };
    setupKnob (gateReleaseSlider, "RELEASE", 10.0, 300.0, 60.0, " ms");
    gateReleaseSlider.onValueChange = [this] { audio.gateReleaseMs.store (static_cast<float> (gateReleaseSlider.getValue())); };

    // 2. Compressor Module
    setupToggle (compToggle, "COMPRESSOR", false);
    compToggle.onStateChange = [this] { audio.compressorEnabled.store (compToggle.getToggleState()); };
    setupKnob (compAmountSlider, "SUSTAIN", 0.0, 1.0, 0.40);
    compAmountSlider.onValueChange = [this] { audio.compressorAmount.store (static_cast<float> (compAmountSlider.getValue())); };
    setupKnob (compAttackSlider, "ATTACK", 2.0, 50.0, 15.0, " ms");
    compAttackSlider.onValueChange = [this] { audio.compressorAttackMs.store (static_cast<float> (compAttackSlider.getValue())); };

    // 3. Overdrive Module
    setupToggle (driveToggle, "OVERDRIVE", true);
    driveToggle.onStateChange = [this] { audio.overdriveEnabled.store (driveToggle.getToggleState()); };
    driveVoicingSelector.addItem ("TS-9 Screamer (Mid Boost)", 1);
    driveVoicingSelector.addItem ("Klon Centaur (Germanium)", 2);
    driveVoicingSelector.addItem ("Rodent Distortion (Hard Clip)", 3);
    driveVoicingSelector.addItem ("Boss HM-2 Chainsaw (Dual Mid)", 4);
    driveVoicingSelector.addItem ("Horizon Precision Drive (Djent)", 5);
    driveVoicingSelector.addItem ("EHX Big Muff Pi (Fuzz)", 6);
    driveVoicingSelector.addItem ("Custom Imported Pedal...", 7);
    driveVoicingSelector.setSelectedId (1, juce::dontSendNotification);
    driveVoicingSelector.onChange = [this]
    {
        int id = driveVoicingSelector.getSelectedId();
        if (id > 0)
        {
            audio.overdriveVoicing.store (id - 1);
        }
    };
    addAndMakeVisible (driveVoicingSelector);

    loadPedalButton.setTooltip ("Load custom distortion pedal profile (.tfpedal, .xml, .wav, .nam)");
    loadPedalButton.onClick = [this] { browseCustomPedal(); };
    addAndMakeVisible (loadPedalButton);

    setupKnob (driveGainSlider, "DRIVE", 0.0, 1.0, 0.50);
    driveGainSlider.onValueChange = [this] { audio.overdriveGain.store (static_cast<float> (driveGainSlider.getValue())); };
    setupKnob (driveToneSlider, "TONE", 0.0, 1.0, 0.65);
    driveToneSlider.onValueChange = [this] { audio.overdriveTone.store (static_cast<float> (driveToneSlider.getValue())); };
    setupKnob (driveLevelSlider, "LEVEL", 0.0, 1.0, 0.85);
    driveLevelSlider.onValueChange = [this] { audio.overdriveLevel.store (static_cast<float> (driveLevelSlider.getValue())); };

    // 4. Amp Head Module
    setupToggle (ampToggle, "TUBE AMP HEAD", true);
    ampToggle.onStateChange = [this] { audio.ampEnabled.store (ampToggle.getToggleState()); };
    ampVoicingSelector.addItem ("Clean (Fender Twin)", 1);
    ampVoicingSelector.addItem ("Crunch (Plexi JCM)", 2);
    ampVoicingSelector.addItem ("Lead (High Gain 5150)", 3);
    ampVoicingSelector.setSelectedId (3, juce::dontSendNotification);
    ampVoicingSelector.onChange = [this] { audio.ampChannel.store (ampVoicingSelector.getSelectedId() - 1); };
    addAndMakeVisible (ampVoicingSelector);

    setupKnob (ampGainSlider, "GAIN", 0.0, 1.0, 0.72);
    ampGainSlider.onValueChange = [this] { audio.ampGain.store (static_cast<float> (ampGainSlider.getValue())); };
    setupKnob (ampBassSlider, "BASS", 0.0, 1.0, 0.55);
    ampBassSlider.onValueChange = [this] { audio.ampBass.store (static_cast<float> (ampBassSlider.getValue())); };
    setupKnob (ampMidSlider, "MIDDLE", 0.0, 1.0, 0.50);
    ampMidSlider.onValueChange = [this] { audio.ampMiddle.store (static_cast<float> (ampMidSlider.getValue())); };
    setupKnob (ampTrebleSlider, "TREBLE", 0.0, 1.0, 0.65);
    ampTrebleSlider.onValueChange = [this] { audio.ampTreble.store (static_cast<float> (ampTrebleSlider.getValue())); };
    setupKnob (ampPresenceSlider, "PRESENCE", 0.0, 1.0, 0.60);
    ampPresenceSlider.onValueChange = [this] { audio.ampPresence.store (static_cast<float> (ampPresenceSlider.getValue())); };
    setupKnob (ampMasterSlider, "MASTER", 0.0, 1.0, 0.75);
    ampMasterSlider.onValueChange = [this] { audio.ampMaster.store (static_cast<float> (ampMasterSlider.getValue())); };

    // 5. Cabinet / IR Module
    setupToggle (cabToggle, "CABINET / IR", true);
    cabToggle.onStateChange = [this] { audio.cabinetEnabled.store (cabToggle.getToggleState()); };
    cabModelSelector.addItem ("4x12 Vintage 30 (Punchy)", 1);
    cabModelSelector.addItem ("2x12 British Greenback", 2);
    cabModelSelector.addItem ("1x12 American Deluxe", 3);
    cabModelSelector.addItem ("Custom IR (.wav file)", 4);
    cabModelSelector.setSelectedId (1, juce::dontSendNotification);
    cabModelSelector.onChange = [this] { audio.cabModel.store (static_cast<int> (cabModelSelector.getSelectedId() - 1)); };
    addAndMakeVisible (cabModelSelector);

    loadIrButton.onClick = [this] { browseCustomIr(); };
    addAndMakeVisible (loadIrButton);

    irFileNameLabel.setText ("Built-in V30 IR Active", juce::dontSendNotification);
    irFileNameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    irFileNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8f94a4));
    addAndMakeVisible (irFileNameLabel);

    // 6. Delay Module
    setupToggle (delayToggle, "STEREO DELAY", true);
    delayToggle.onStateChange = [this] { audio.delayEnabled.store (delayToggle.getToggleState()); };
    setupKnob (delayTimeSlider, "TIME", 20.0, 1000.0, 380.0, " ms");
    delayTimeSlider.onValueChange = [this] { audio.delayTimeMs.store (static_cast<float> (delayTimeSlider.getValue())); };
    setupKnob (delayFeedbackSlider, "FEEDBACK", 0.0, 0.9, 0.32);
    delayFeedbackSlider.onValueChange = [this] { audio.delayFeedback.store (static_cast<float> (delayFeedbackSlider.getValue())); };
    setupKnob (delayToneSlider, "TONE", 0.0, 1.0, 0.60);
    delayToneSlider.onValueChange = [this] { audio.delayTone.store (static_cast<float> (delayToneSlider.getValue())); };
    setupKnob (delayMixSlider, "MIX", 0.0, 0.8, 0.22);
    delayMixSlider.onValueChange = [this] { audio.delayMix.store (static_cast<float> (delayMixSlider.getValue())); };

    // 7. Reverb Module
    setupToggle (reverbToggle, "LUSH REVERB", true);
    reverbToggle.onStateChange = [this] { audio.reverbEnabled.store (reverbToggle.getToggleState()); };
    loadReverbIrButton.setTooltip ("Load custom studio/reverb space impulse response (.wav)");
    loadReverbIrButton.onClick = [this] { browseCustomReverbIr(); };
    addAndMakeVisible (loadReverbIrButton);

    reverbFileNameLabel.setText ("Built-in Plate/Hall", juce::dontSendNotification);
    reverbFileNameLabel.setFont (juce::Font (juce::FontOptions (10.5f)));
    reverbFileNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8f94a4));
    addAndMakeVisible (reverbFileNameLabel);

    setupKnob (reverbDecaySlider, "DECAY", 0.0, 1.0, 0.55);
    reverbDecaySlider.onValueChange = [this] { audio.reverbDecay.store (static_cast<float> (reverbDecaySlider.getValue())); };
    setupKnob (reverbDampingSlider, "DAMPING", 0.0, 1.0, 0.35);
    reverbDampingSlider.onValueChange = [this] { audio.reverbDamping.store (static_cast<float> (reverbDampingSlider.getValue())); };
    setupKnob (reverbMixSlider, "MIX", 0.0, 0.8, 0.18);
    reverbMixSlider.onValueChange = [this] { audio.reverbMix.store (static_cast<float> (reverbMixSlider.getValue())); };

    // 8. External VST3 Plugin Module
    setupToggle (vst3Toggle, "EXTERNAL VST3 PLUGIN", false);
    vst3Toggle.onStateChange = [this] { audio.vst3Enabled.store (vst3Toggle.getToggleState()); };

    loadVst3Button.setTooltip ("Load external 64-bit VST3 audio plugin (.vst3)");
    loadVst3Button.onClick = [this] { browseVst3Plugin(); };
    addAndMakeVisible (loadVst3Button);

    openVst3UiButton.setTooltip ("Open graphical UI window for loaded VST3 plugin");
    openVst3UiButton.onClick = [this] { openVst3EditorWindow(); };
    addAndMakeVisible (openVst3UiButton);

    unloadVst3Button.setTooltip ("Unload current VST3 plugin");
    unloadVst3Button.onClick = [this] { unloadVst3Plugin(); };
    addAndMakeVisible (unloadVst3Button);

    vst3PluginNameLabel.setText ("No VST3 Plugin Loaded", juce::dontSendNotification);
    vst3PluginNameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    vst3PluginNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8f94a4));
    addAndMakeVisible (vst3PluginNameLabel);

    // 9. Neural Amp Modeler (NAM) Module
    setupToggle (namToggle, "NEURAL AMP MODELER (NAM)", false);
    namToggle.onStateChange = [this] { audio.namEnabled.store (namToggle.getToggleState()); };

    namModelSelector.onChange = [this]
    {
        int id = namModelSelector.getSelectedId();
        if (id <= 1)
            return;

        int browseId = static_cast<int> (discoveredNamModels.size() + 2);
        if (id == browseId)
        {
            browseNamModel();
        }
        else if (id >= 2 && id < browseId)
        {
            size_t idx = static_cast<size_t> (id - 2);
            if (idx < discoveredNamModels.size())
            {
                juce::String err;
                if (audio.loadNamModel (discoveredNamModels[idx].file, err))
                {
                    namToggle.setToggleState (true, juce::sendNotification);
                    namModelNameLabel.setText (audio.namModelName, juce::dontSendNotification);
                }
                else
                {
                    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                             "NAM Model Load Error",
                                                             err, "OK", this);
                }
            }
        }
    };
    addAndMakeVisible (namModelSelector);

    openModelsFolderButton.setTooltip ("Open local models folder in Windows Explorer to drop or paste .nam files");
    openModelsFolderButton.onClick = [this] { openModelsFolder(); };
    addAndMakeVisible (openModelsFolderButton);

    getToneHuntButton.setTooltip ("Open ToneHunt.org in browser to download thousands of free community NAM models");
    getToneHuntButton.onClick = [this] { openToneHuntWebsite(); };
    addAndMakeVisible (getToneHuntButton);

    unloadNamButton.setTooltip ("Unload current NAM model");
    unloadNamButton.onClick = [this] { unloadNamModel(); };
    addAndMakeVisible (unloadNamButton);

    namModelNameLabel.setText ("No NAM Model Loaded", juce::dontSendNotification);
    namModelNameLabel.setFont (juce::Font (juce::FontOptions (11.0f)));
    namModelNameLabel.setColour (juce::Label::textColourId, juce::Colour (0xff8f94a4));
    addAndMakeVisible (namModelNameLabel);

    setupKnob (namInputGainSlider, "IN GAIN", -18.0, 18.0, 0.0, " dB");
    namInputGainSlider.onValueChange = [this] { audio.namInputGainDb.store (static_cast<float> (namInputGainSlider.getValue())); };

    setupKnob (namOutputGainSlider, "OUT GAIN", -18.0, 18.0, 0.0, " dB");
    namOutputGainSlider.onValueChange = [this] { audio.namOutputGainDb.store (static_cast<float> (namOutputGainSlider.getValue())); };

    refreshNamModelList();

    // Footer Controls
    setupKnob (inputTrimSlider, "IN GAIN", -24.0, 24.0, 0.0, " dB");
    inputTrimSlider.onValueChange = [this] { audio.inputTrimDb.store (static_cast<float> (inputTrimSlider.getValue())); };

    inputMeterLabel.setText ("IN: -inf dB", juce::dontSendNotification);
    inputMeterLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    inputMeterLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa2a6b2));
    addAndMakeVisible (inputMeterLabel);

    hardwareStatusLabel.setText ("Focusrite Solo 4th Gen • ASIO Ready", juce::dontSendNotification);
    hardwareStatusLabel.setFont (juce::Font (juce::FontOptions (13.0f, juce::Font::bold)));
    hardwareStatusLabel.setJustificationType (juce::Justification::centred);
    hardwareStatusLabel.setColour (juce::Label::textColourId, juce::Colour (0xff0ba4f0));
    addAndMakeVisible (hardwareStatusLabel);

    setupKnob (outputSlider, "MASTER VOL", -60.0, 6.0, -50.0, " dB");
    outputSlider.onValueChange = [this] { audio.outputDb.store (static_cast<float> (outputSlider.getValue())); };

    outputMeterLabel.setText ("OUT: -inf dB", juce::dontSendNotification);
    outputMeterLabel.setFont (juce::Font (juce::FontOptions (12.0f, juce::Font::bold)));
    outputMeterLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa2a6b2));
    addAndMakeVisible (outputMeterLabel);

    // Load presets and restore last used profile/preset after all controls and sliders are initialized
    juce::String lastPresetName;
    if (auto* userSettings = appProperties.getUserSettings())
        lastPresetName = userSettings->getValue ("lastPreset");

    refreshPresetList (lastPresetName);

    presetSelector.onChange = [this]
    {
        int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < static_cast<int> (presets.size()))
        {
            applyPreset (idx);
            if (auto* userSettings = appProperties.getUserSettings())
            {
                userSettings->setValue ("lastPreset", presets[static_cast<size_t> (idx)].name);
                appProperties.saveIfNeeded();
            }
        }
    };

    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    saveCurrentPresetState();
    if (vst3Window != nullptr)
        vst3Window.reset();
    setLookAndFeel (nullptr);
}

void MainPanel::setupKnob (juce::Slider& slider, const juce::String& text, double min, double max, double init, const juce::String& suffix)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    slider.setRange (min, max, (max - min) > 10.0 ? 0.5 : 0.01);
    slider.setValue (init, juce::dontSendNotification);
    slider.setTextValueSuffix (suffix);
    addAndMakeVisible (slider);

    auto* lbl = new juce::Label();
    lbl->setText (text, juce::dontSendNotification);
    lbl->setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
    lbl->setJustificationType (juce::Justification::centred);
    lbl->setColour (juce::Label::textColourId, juce::Colour (0xffb2b6c3));
    lbl->setInterceptsMouseClicks (false, false);
    addAndMakeVisible (lbl);
    knobLabels.add (lbl);
    sliderLabelMap[&slider] = lbl;
}

void MainPanel::layoutKnob (juce::Rectangle<int> bounds, juce::Slider& slider)
{
    auto it = sliderLabelMap.find (&slider);
    if (it != sliderLabelMap.end() && it->second != nullptr)
    {
        it->second->setBounds (bounds.removeFromTop (16));
    }
    slider.setBounds (bounds.reduced (1, 0));
}

void MainPanel::drawModuleCard (juce::Graphics& g, juce::Rectangle<int> bounds, bool isEnabled, const juce::Colour& accentColour)
{
    if (bounds.isEmpty())
        return;

    auto fBounds = bounds.toFloat();
    const float corner = 6.0f;

    // Card chassis body
    juce::Colour cardBg = isEnabled ? juce::Colour (0xff202228) : juce::Colour (0xff191a1c);
    g.setColour (cardBg);
    g.fillRoundedRectangle (fBounds, corner);

    // Card header bar plate
    auto headerBar = bounds.removeFromTop (28).toFloat();
    juce::Path headerPath;
    headerPath.addRoundedRectangle (headerBar.getX(), headerBar.getY(), headerBar.getWidth(), headerBar.getHeight(),
                                    corner, corner, true, true, false, false);
    g.setColour (isEnabled ? juce::Colour (0xff282b33) : juce::Colour (0xff1e2025));
    g.fillPath (headerPath);

    // Subtle header bottom divider line
    g.setColour (juce::Colour (0xff333742));
    g.drawHorizontalLine (bounds.getY() + 28, fBounds.getX(), fBounds.getRight());

    // Card outline border
    juce::Colour borderCol = isEnabled ? juce::Colour (0xff393d4a) : juce::Colour (0xff252830);
    g.setColour (borderCol);
    g.drawRoundedRectangle (fBounds, corner, 1.0f);

    // Subtle top accent line if enabled
    if (isEnabled)
    {
        g.setColour (accentColour.withAlpha (0.75f));
        g.drawLine (fBounds.getX() + corner, fBounds.getY() + 0.5f, fBounds.getRight() - corner, fBounds.getY() + 0.5f, 1.2f);
    }
}

void MainPanel::setupToggle (juce::ToggleButton& btn, const juce::String& text, bool init)
{
    btn.setButtonText (text);
    btn.setToggleState (init, juce::dontSendNotification);
    addAndMakeVisible (btn);
}

void MainPanel::applyPreset (int index)
{
    if (index < 0 || index >= static_cast<int> (presets.size()))
        return;

    const auto& p = presets[static_cast<size_t> (index)];

    gateToggle.setToggleState (p.gateOn, juce::sendNotification);
    gateThresholdSlider.setValue (p.gateThreshDb, juce::sendNotification);
    gateReleaseSlider.setValue (p.gateReleaseMs, juce::sendNotification);

    compToggle.setToggleState (p.compOn, juce::sendNotification);
    compAmountSlider.setValue (p.compAmount, juce::sendNotification);
    compAttackSlider.setValue (p.compAttackMs, juce::sendNotification);

    driveToggle.setToggleState (p.driveOn, juce::sendNotification);
    driveVoicingSelector.setSelectedId (p.driveVoicing + 1, juce::dontSendNotification);
    audio.overdriveVoicing.store (p.driveVoicing);
    driveGainSlider.setValue (p.driveGain, juce::sendNotification);
    driveToneSlider.setValue (p.driveTone, juce::sendNotification);
    driveLevelSlider.setValue (p.driveLevel, juce::sendNotification);
    if (p.driveVoicing == 6 && p.customPedalPath.isNotEmpty())
    {
        juce::File pedalFile (p.customPedalPath);
        if (pedalFile.existsAsFile())
            audio.loadCustomPedalFile (pedalFile);
    }

    ampToggle.setToggleState (p.ampOn, juce::sendNotification);
    ampVoicingSelector.setSelectedId (p.ampChannel + 1, juce::dontSendNotification);
    audio.ampChannel.store (p.ampChannel);
    ampGainSlider.setValue (p.ampGain, juce::sendNotification);
    ampBassSlider.setValue (p.ampBass, juce::sendNotification);
    ampMidSlider.setValue (p.ampMiddle, juce::sendNotification);
    ampTrebleSlider.setValue (p.ampTreble, juce::sendNotification);
    ampPresenceSlider.setValue (p.ampPresence, juce::sendNotification);
    ampMasterSlider.setValue (p.ampMaster, juce::sendNotification);

    cabToggle.setToggleState (p.cabOn, juce::sendNotification);
    cabModelSelector.setSelectedId (p.cabModel + 1, juce::dontSendNotification);
    audio.cabModel.store (p.cabModel);
    if (p.cabModel == 3 && p.customIrPath.isNotEmpty())
    {
        juce::File irFile (p.customIrPath);
        if (! irFile.existsAsFile())
        {
            auto appDataIrs = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                    .getChildFile ("ToneForge")
                                    .getChildFile ("IRs");
            irFile = appDataIrs.getChildFile (juce::File (p.customIrPath).getFileName());

            if (! irFile.existsAsFile())
            {
                auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
                irFile = exeDir.getChildFile ("Resources").getChildFile ("IRs").getChildFile (juce::File (p.customIrPath).getFileName());
                if (! irFile.existsAsFile())
                    irFile = juce::File ("E:/ToneForge/Resources/IRs").getChildFile (juce::File (p.customIrPath).getFileName());
            }
        }

        if (irFile.existsAsFile())
        {
            if (audio.loadCustomIrFile (irFile))
            {
                cabModelSelector.setSelectedId (4, juce::dontSendNotification);
                audio.cabModel.store (static_cast<int> (CabType::CustomIR));
                irFileNameLabel.setText (irFile.getFileName(), juce::dontSendNotification);
            }
        }
    }

    delayToggle.setToggleState (p.delayOn, juce::sendNotification);
    delayTimeSlider.setValue (p.delayTimeMs, juce::sendNotification);
    delayFeedbackSlider.setValue (p.delayFeedback, juce::sendNotification);
    delayToneSlider.setValue (p.delayTone, juce::sendNotification);
    delayMixSlider.setValue (p.delayMix, juce::sendNotification);

    reverbToggle.setToggleState (p.reverbOn, juce::sendNotification);
    reverbDecaySlider.setValue (p.reverbDecay, juce::sendNotification);
    reverbDampingSlider.setValue (p.reverbDamping, juce::sendNotification);
    reverbMixSlider.setValue (p.reverbMix, juce::sendNotification);
    if (p.customReverbPath.isNotEmpty())
    {
        juce::File revFile (p.customReverbPath);
        if (revFile.existsAsFile())
        {
            if (audio.loadCustomReverbIrFile (revFile))
                reverbFileNameLabel.setText (revFile.getFileName(), juce::dontSendNotification);
        }
    }
    else
    {
        audio.hasCustomReverbLoaded = false;
        reverbFileNameLabel.setText ("Built-in Plate/Hall", juce::dontSendNotification);
    }

    vst3Toggle.setToggleState (p.vst3On, juce::sendNotification);
    if (p.vst3PluginPath.isNotEmpty())
    {
        juce::File vstFile (p.vst3PluginPath);
        if (vstFile.exists())
        {
            juce::String err;
            if (audio.loadVst3Plugin (vstFile, err))
                vst3PluginNameLabel.setText (audio.vst3PluginName, juce::dontSendNotification);
        }
    }

    namToggle.setToggleState (p.namOn, juce::sendNotification);
    namInputGainSlider.setValue (p.namInputGainDb, juce::sendNotification);
    namOutputGainSlider.setValue (p.namOutputGainDb, juce::sendNotification);
    if (p.namModelPath.isNotEmpty())
    {
        juce::File namFile (p.namModelPath);
        if (! namFile.existsAsFile())
        {
            auto appDataModels = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                        .getChildFile ("ToneForge")
                                        .getChildFile ("Models");
            namFile = appDataModels.getChildFile (juce::File (p.namModelPath).getFileName());

            if (! namFile.existsAsFile())
            {
                auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
                namFile = exeDir.getChildFile ("Resources").getChildFile ("Models").getChildFile (juce::File (p.namModelPath).getFileName());
                if (! namFile.existsAsFile())
                    namFile = juce::File ("E:/ToneForge/Resources/Models").getChildFile (juce::File (p.namModelPath).getFileName());
            }
        }

        if (namFile.existsAsFile())
        {
            juce::String err;
            if (audio.loadNamModel (namFile, err))
            {
                namModelNameLabel.setText (audio.namModelName, juce::dontSendNotification);
                refreshNamModelList (namFile.getFullPathName());
            }
        }
    }
    else
    {
        audio.unloadNamModel();
        namModelNameLabel.setText ("No NAM Model Loaded", juce::dontSendNotification);
        refreshNamModelList ("");
    }

    outputSlider.setValue (p.outputDb, juce::sendNotification);
}

void MainPanel::promptSaveProfile()
{
    int currentIdx = presetSelector.getSelectedId() - 1;
    juce::String defaultName = "My Custom Profile";
    if (currentIdx >= 0 && currentIdx < static_cast<int> (presets.size()))
    {
        const auto& cur = presets[static_cast<size_t> (currentIdx)];
        if (cur.isUserPreset)
            defaultName = cur.name;
        else
            defaultName = cur.name + " (Custom)";
    }

    auto* alert = new juce::AlertWindow ("SAVE PROFILE",
                                         "Enter a name for your custom guitar profile:",
                                         juce::AlertWindow::NoIcon);
    alert->setLookAndFeel (&lookAndFeel);
    alert->addTextEditor ("profileName", defaultName, "Profile Name:");
    if (auto* ed = alert->getTextEditor ("profileName"))
        ed->selectAll();

    alert->addButton ("SAVE", 1, juce::KeyPress (juce::KeyPress::returnKey));
    alert->addButton ("CANCEL", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    alert->enterModalState (true, juce::ModalCallbackFunction::create ([this, alert] (int result)
    {
        if (result == 1)
        {
            auto name = alert->getTextEditorContents ("profileName").trim();
            if (name.isNotEmpty())
                saveProfileWithName (name);
        }
    }), true);
}

void MainPanel::saveProfileWithName (const juce::String& name)
{
    TonePreset p;
    p.name = name;
    p.isUserPreset = true;

    p.gateOn = gateToggle.getToggleState();
    p.gateThreshDb = static_cast<float> (gateThresholdSlider.getValue());
    p.gateReleaseMs = static_cast<float> (gateReleaseSlider.getValue());

    p.compOn = compToggle.getToggleState();
    p.compAmount = static_cast<float> (compAmountSlider.getValue());
    p.compAttackMs = static_cast<float> (compAttackSlider.getValue());

    p.driveOn = driveToggle.getToggleState();
    p.driveVoicing = driveVoicingSelector.getSelectedId() - 1;
    p.driveGain = static_cast<float> (driveGainSlider.getValue());
    p.driveTone = static_cast<float> (driveToneSlider.getValue());
    p.driveLevel = static_cast<float> (driveLevelSlider.getValue());
    p.customPedalPath = audio.customPedalPath;

    p.ampOn = ampToggle.getToggleState();
    p.ampChannel = ampVoicingSelector.getSelectedId() - 1;
    p.ampGain = static_cast<float> (ampGainSlider.getValue());
    p.ampBass = static_cast<float> (ampBassSlider.getValue());
    p.ampMiddle = static_cast<float> (ampMidSlider.getValue());
    p.ampTreble = static_cast<float> (ampTrebleSlider.getValue());
    p.ampPresence = static_cast<float> (ampPresenceSlider.getValue());
    p.ampMaster = static_cast<float> (ampMasterSlider.getValue());

    p.cabOn = cabToggle.getToggleState();
    p.cabModel = cabModelSelector.getSelectedId() - 1;
    p.customIrPath = audio.customIrPath;

    p.delayOn = delayToggle.getToggleState();
    p.delayTimeMs = static_cast<float> (delayTimeSlider.getValue());
    p.delayFeedback = static_cast<float> (delayFeedbackSlider.getValue());
    p.delayTone = static_cast<float> (delayToneSlider.getValue());
    p.delayMix = static_cast<float> (delayMixSlider.getValue());

    p.reverbOn = reverbToggle.getToggleState();
    p.reverbDecay = static_cast<float> (reverbDecaySlider.getValue());
    p.reverbDamping = static_cast<float> (reverbDampingSlider.getValue());
    p.reverbMix = static_cast<float> (reverbMixSlider.getValue());
    p.customReverbPath = audio.customReverbPath;

    p.vst3On = vst3Toggle.getToggleState();
    p.vst3PluginPath = audio.vst3PluginPath;

    p.namOn = namToggle.getToggleState();
    p.namModelPath = audio.namModelPath;
    p.namInputGainDb = static_cast<float> (namInputGainSlider.getValue());
    p.namOutputGainDb = static_cast<float> (namOutputGainSlider.getValue());

    p.outputDb = static_cast<float> (outputSlider.getValue());

    if (PresetManager::saveUserPreset (p))
    {
        refreshPresetList (name);
        if (auto* userSettings = appProperties.getUserSettings())
        {
            userSettings->setValue ("lastPreset", name);
            appProperties.saveIfNeeded();
        }
    }
}

void MainPanel::saveCurrentPresetState()
{
    int idx = presetSelector.getSelectedId() - 1;
    if (idx >= 0 && idx < static_cast<int> (presets.size()))
    {
        if (auto* userSettings = appProperties.getUserSettings())
        {
            userSettings->setValue ("lastPreset", presets[static_cast<size_t> (idx)].name);
            appProperties.saveIfNeeded();
        }

        if (presets[static_cast<size_t> (idx)].isUserPreset)
        {
            TonePreset p;
            p.name = presets[static_cast<size_t> (idx)].name;
            p.isUserPreset = true;

            p.gateOn = gateToggle.getToggleState();
            p.gateThreshDb = static_cast<float> (gateThresholdSlider.getValue());
            p.gateReleaseMs = static_cast<float> (gateReleaseSlider.getValue());

            p.compOn = compToggle.getToggleState();
            p.compAmount = static_cast<float> (compAmountSlider.getValue());
            p.compAttackMs = static_cast<float> (compAttackSlider.getValue());

            p.driveOn = driveToggle.getToggleState();
            p.driveVoicing = driveVoicingSelector.getSelectedId() - 1;
            p.driveGain = static_cast<float> (driveGainSlider.getValue());
            p.driveTone = static_cast<float> (driveToneSlider.getValue());
            p.driveLevel = static_cast<float> (driveLevelSlider.getValue());
            p.customPedalPath = audio.customPedalPath;

            p.ampOn = ampToggle.getToggleState();
            p.ampChannel = ampVoicingSelector.getSelectedId() - 1;
            p.ampGain = static_cast<float> (ampGainSlider.getValue());
            p.ampBass = static_cast<float> (ampBassSlider.getValue());
            p.ampMiddle = static_cast<float> (ampMidSlider.getValue());
            p.ampTreble = static_cast<float> (ampTrebleSlider.getValue());
            p.ampPresence = static_cast<float> (ampPresenceSlider.getValue());
            p.ampMaster = static_cast<float> (ampMasterSlider.getValue());

            p.cabOn = cabToggle.getToggleState();
            p.cabModel = cabModelSelector.getSelectedId() - 1;
            p.customIrPath = audio.customIrPath;

            p.delayOn = delayToggle.getToggleState();
            p.delayTimeMs = static_cast<float> (delayTimeSlider.getValue());
            p.delayFeedback = static_cast<float> (delayFeedbackSlider.getValue());
            p.delayTone = static_cast<float> (delayToneSlider.getValue());
            p.delayMix = static_cast<float> (delayMixSlider.getValue());

            p.reverbOn = reverbToggle.getToggleState();
            p.reverbDecay = static_cast<float> (reverbDecaySlider.getValue());
            p.reverbDamping = static_cast<float> (reverbDampingSlider.getValue());
            p.reverbMix = static_cast<float> (reverbMixSlider.getValue());
            p.customReverbPath = audio.customReverbPath;

            p.vst3On = vst3Toggle.getToggleState();
            p.vst3PluginPath = audio.vst3PluginPath;

            p.namOn = namToggle.getToggleState();
            p.namModelPath = audio.namModelPath;
            p.namInputGainDb = static_cast<float> (namInputGainSlider.getValue());
            p.namOutputGainDb = static_cast<float> (namOutputGainSlider.getValue());

            p.outputDb = static_cast<float> (outputSlider.getValue());

            PresetManager::saveUserPreset (p);
        }
    }
}

void MainPanel::refreshPresetList (const juce::String& presetNameToSelect)
{
    presets = PresetManager::getAllPresets();

    presetSelector.clear (juce::dontSendNotification);

    auto factory = PresetManager::getFactoryPresets();
    presetSelector.addSectionHeading ("FACTORY PRESETS");
    for (size_t i = 0; i < factory.size(); ++i)
        presetSelector.addItem (presets[i].name, static_cast<int> (i + 1));

    if (presets.size() > factory.size())
    {
        presetSelector.addSeparator();
        presetSelector.addSectionHeading ("USER PROFILES");
        for (size_t i = factory.size(); i < presets.size(); ++i)
            presetSelector.addItem (presets[i].name, static_cast<int> (i + 1));
    }

    int targetIdx = 0;
    if (presetNameToSelect.isNotEmpty())
    {
        for (size_t i = 0; i < presets.size(); ++i)
        {
            if (presets[i].name.equalsIgnoreCase (presetNameToSelect))
            {
                targetIdx = static_cast<int> (i);
                break;
            }
        }
    }

    presetSelector.setSelectedId (targetIdx + 1, juce::dontSendNotification);
    applyPreset (targetIdx);

    if (auto* userSettings = appProperties.getUserSettings())
    {
        userSettings->setValue ("lastPreset", presets[static_cast<size_t> (targetIdx)].name);
        appProperties.saveIfNeeded();
    }
}

void MainPanel::openAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager,
                                                             1, 2,  // min/max inputs
                                                             1, 2,  // min/max outputs
                                                             true,  // show MIDI in
                                                             false, // show MIDI out
                                                             true,  // show channels as stereo pairs
                                                             false); // hide advanced
    selector->setSize (540, 480);

    juce::DialogWindow::LaunchOptions opt;
    opt.dialogTitle = "ToneForge - Audio Device Configuration";
    opt.content.setOwned (selector);
    opt.dialogBackgroundColour = juce::Colour (0xff191a1c);
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = true;
    opt.launchAsync();
}

void MainPanel::browseCustomIr()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Guitar Cabinet Impulse Response (.wav)",
                                                        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                        "*.wav");
    auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (folderChooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            if (audio.loadCustomIrFile (file))
            {
                cabModelSelector.setSelectedId (4, juce::dontSendNotification);
                audio.cabModel.store (static_cast<int> (CabType::CustomIR));
                irFileNameLabel.setText (file.getFileName(), juce::dontSendNotification);
            }
        }
    });
}

void MainPanel::browseCustomPedal()
{
    auto userPedalFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("ToneForge")
                                .getChildFile ("Pedals");
    if (! userPedalFolder.exists())
        userPedalFolder.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Select Distortion Pedal File (.tfpedal, .xml, .wav, .nam)",
                                                        userPedalFolder.exists() ? userPedalFolder : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                        "*.tfpedal;*.xml;*.wav;*.nam;*.json");
    auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (folderChooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            if (audio.loadCustomPedalFile (file))
            {
                driveVoicingSelector.setSelectedId (7, juce::dontSendNotification);
                driveToggle.setToggleState (true, juce::sendNotification);
            }
        }
    });
}

void MainPanel::browseCustomReverbIr()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select Reverb Impulse Response (.wav)",
                                                        juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                        "*.wav");
    auto folderChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (folderChooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            if (audio.loadCustomReverbIrFile (file))
            {
                reverbToggle.setToggleState (true, juce::sendNotification);
                reverbFileNameLabel.setText (file.getFileName(), juce::dontSendNotification);
            }
        }
    });
}

void MainPanel::browseVst3Plugin()
{
    juce::File defaultVst3Dir ("C:\\Program Files\\Common Files\\VST3");
    if (! defaultVst3Dir.exists())
        defaultVst3Dir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    fileChooser = std::make_unique<juce::FileChooser> ("Select 64-bit VST3 Plugin File or Bundle (.vst3)",
                                                        defaultVst3Dir,
                                                        "*.vst3");
    auto vstChooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories;
    fileChooser->launchAsync (vstChooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.exists())
        {
            juce::String err;
            if (audio.loadVst3Plugin (file, err))
            {
                vst3Toggle.setToggleState (true, juce::sendNotification);
                vst3PluginNameLabel.setText (audio.vst3PluginName, juce::dontSendNotification);
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                         "VST3 Plugin Load Error",
                                                         err, "OK", this);
            }
        }
    });
}

void MainPanel::openVst3EditorWindow()
{
    if (! audio.isVst3PluginLoaded())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                 "No VST3 Plugin Loaded",
                                                 "Please click 'LOAD VST3...' to load an external plugin first.",
                                                 "OK", this);
        return;
    }

    if (auto* editor = audio.createVst3PluginEditor())
    {
        vst3Window = std::make_unique<Vst3PluginWindow> (audio.vst3PluginName, editor);
        vst3Window->setVisible (true);
        vst3Window->toFront (true);
    }
    else
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::InfoIcon,
                                                 "No Graphical Interface",
                                                 "The loaded plugin does not provide a custom GUI editor.",
                                                 "OK", this);
    }
}

void MainPanel::unloadVst3Plugin()
{
    if (vst3Window != nullptr)
        vst3Window.reset();

    audio.unloadVst3Plugin();
    vst3Toggle.setToggleState (false, juce::sendNotification);
    vst3PluginNameLabel.setText ("No VST3 Plugin Loaded", juce::dontSendNotification);
}

void MainPanel::refreshNamModelList (const juce::String& modelPathToSelect)
{
    discoveredNamModels.clear();
    namModelSelector.clear (juce::dontSendNotification);

    std::vector<juce::File> searchDirs;

    // AppData/Roaming/ToneForge/Models
    auto appDataModels = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("ToneForge")
                                .getChildFile ("Models");
    if (! appDataModels.exists())
        appDataModels.createDirectory();
    searchDirs.push_back (appDataModels);

    // Documents/NAM Models & Documents/NeuralAmpModeler
    auto userDocs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    searchDirs.push_back (userDocs.getChildFile ("NAM Models"));
    searchDirs.push_back (userDocs.getChildFile ("NeuralAmpModeler"));
    searchDirs.push_back (userDocs.getChildFile ("Neural Amp Modeler"));

    // User Downloads
    auto downloads = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads");
    if (downloads.isDirectory())
        searchDirs.push_back (downloads);

    std::unordered_set<juce::String> seenPaths;

    for (const auto& dir : searchDirs)
    {
        if (dir.isDirectory())
        {
            auto files = dir.findChildFiles (juce::File::findFiles, false, "*.nam;*.json");
            for (const auto& f : files)
            {
                auto path = f.getFullPathName();
                if (seenPaths.find (path) == seenPaths.end())
                {
                    seenPaths.insert (path);
                    DiscoveredNamModel item;
                    item.name = f.getFileNameWithoutExtension();
                    item.file = f;
                    discoveredNamModels.push_back (item);
                }
            }
        }
    }

    std::sort (discoveredNamModels.begin(), discoveredNamModels.end(), [] (const DiscoveredNamModel& a, const DiscoveredNamModel& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    namModelSelector.addItem ("-- Select NAM Model --", 1);
    int selectedId = 1;

    for (size_t i = 0; i < discoveredNamModels.size(); ++i)
    {
        int itemId = static_cast<int> (i + 2);
        namModelSelector.addItem (discoveredNamModels[i].name, itemId);

        if (modelPathToSelect.isNotEmpty() && discoveredNamModels[i].file.getFullPathName().equalsIgnoreCase (modelPathToSelect))
            selectedId = itemId;
    }

    namModelSelector.addSeparator();
    namModelSelector.addItem ("Browse for File...", static_cast<int> (discoveredNamModels.size() + 2));

    namModelSelector.setSelectedId (selectedId, juce::dontSendNotification);
}

void MainPanel::openModelsFolder()
{
    auto appDataModels = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("ToneForge")
                                .getChildFile ("Models");
    if (! appDataModels.exists())
        appDataModels.createDirectory();

    appDataModels.revealToUser();
}

void MainPanel::openToneHuntWebsite()
{
    juce::URL ("https://tonehunt.org").launchInDefaultBrowser();
}

void MainPanel::browseNamModel()
{
    auto userModelFolder = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                .getChildFile ("ToneForge")
                                .getChildFile ("Models");
    if (! userModelFolder.exists())
        userModelFolder.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> ("Select Neural Amp Modeler File (.nam, .json)",
                                                        userModelFolder.exists() ? userModelFolder : juce::File::getSpecialLocation (juce::File::userHomeDirectory),
                                                        "*.nam;*.json");
    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            juce::String err;
            if (audio.loadNamModel (file, err))
            {
                namToggle.setToggleState (true, juce::sendNotification);
                namModelNameLabel.setText (audio.namModelName, juce::dontSendNotification);
                refreshNamModelList (file.getFullPathName());
            }
            else
            {
                juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                         "NAM Model Load Error",
                                                         err, "OK", this);
            }
        }
    });
}

void MainPanel::unloadNamModel()
{
    audio.unloadNamModel();
    namToggle.setToggleState (false, juce::sendNotification);
    namModelNameLabel.setText ("No NAM Model Loaded", juce::dontSendNotification);
    namModelSelector.setSelectedId (1, juce::dontSendNotification);
}

void MainPanel::updateTelemetryString()
{
    if (auto* currentDevice = deviceManager.getCurrentAudioDevice())
    {
        juce::String typeName = deviceManager.getCurrentAudioDeviceType();
        juce::String devName = currentDevice->getName();
        int rate = static_cast<int> (currentDevice->getCurrentSampleRate());
        int bufSize = currentDevice->getCurrentBufferSizeSamples();
        float latencyMs = (static_cast<float> (bufSize) / static_cast<float> (rate)) * 1000.0f;

        hardwareStatusLabel.setText (typeName + ": " + devName + "  •  " +
                                     juce::String (rate) + " Hz  •  " +
                                     juce::String (bufSize) + " spls (" +
                                     juce::String (latencyMs, 1) + " ms latency)",
                                     juce::dontSendNotification);
    }
    else
    {
        hardwareStatusLabel.setText ("No Audio Device Selected • Click AUDIO SETUP", juce::dontSendNotification);
    }
}

void MainPanel::timerCallback()
{
    // Update Tuner HUD
    auto res = audio.tuner.getLatestResult();
    if (res.hasSignal && res.frequency > 50.0f)
    {
        tunerNoteLabel.setText (res.noteName, juce::dontSendNotification);
        tunerNoteLabel.setColour (juce::Label::textColourId, res.inTune ? juce::Colour (0xff38b000) : juce::Colour (0xffffffff));

        visualCents += (res.cents - visualCents) * 0.45f;
        juce::String sign = visualCents >= 0.0f ? "+" : "";
        tunerCentsLabel.setText (res.inTune ? "IN TUNE!" : (sign + juce::String (visualCents, 1) + " cents"), juce::dontSendNotification);
        tunerCentsLabel.setColour (juce::Label::textColourId, res.inTune ? juce::Colour (0xff38b000) : juce::Colour (0xff0ba4f0));

        tunerFreqLabel.setText (juce::String (res.frequency, 1) + " Hz", juce::dontSendNotification);
    }
    else
    {
        tunerNoteLabel.setText ("--", juce::dontSendNotification);
        tunerNoteLabel.setColour (juce::Label::textColourId, juce::Colour (0xff5a6070));
        tunerCentsLabel.setText ("CHROMATIC TUNER", juce::dontSendNotification);
        tunerCentsLabel.setColour (juce::Label::textColourId, juce::Colour (0xff7e8494));
        tunerFreqLabel.setText ("Play guitar to tune", juce::dontSendNotification);
        visualCents = 0.0f;
    }

    // Telemetry updates
    updateTelemetryString();

    // Input & Output Meters
    float inRms = audio.inputRms.load();
    visualInputRms += (inRms - visualInputRms) * 0.3f;
    float inDb = juce::Decibels::gainToDecibels (visualInputRms, -80.0f);
    int activeCh = audio.autoActiveChannel.load();
    inputMeterLabel.setText ("IN (Ch " + juce::String (activeCh) + "): " + (inDb < -75.0f ? "-inf" : juce::String (inDb, 1)) + " dB", juce::dontSendNotification);

    float outRms = audio.outputRms.load();
    visualOutputRms += (outRms - visualOutputRms) * 0.3f;
    float outDb = juce::Decibels::gainToDecibels (visualOutputRms, -80.0f);
    outputMeterLabel.setText ("OUT: " + (outDb < -75.0f ? "-inf" : juce::String (outDb, 1)) + " dB", juce::dontSendNotification);

    repaint();
}

void MainPanel::paint (juce::Graphics& g)
{
    // Deep studio dark background (#191A1C)
    g.fillAll (juce::Colour (0xff191a1c));

    // 1. Header background
    const int headerHeight = 64;
    auto headerArea = juce::Rectangle<int> (0, 0, getWidth(), headerHeight);
    juce::ColourGradient headerGrad (juce::Colour (0xff22242b), 0.0f, 0.0f,
                                     juce::Colour (0xff191a1c), 0.0f, static_cast<float> (headerHeight), false);
    g.setGradientFill (headerGrad);
    g.fillRect (headerArea);
    g.setColour (juce::Colour (0xff333742));
    g.drawHorizontalLine (headerHeight - 1, 0.0f, static_cast<float> (getWidth()));

    // 2. Footer background
    const int footerHeight = 46;
    auto footerArea = juce::Rectangle<int> (0, getHeight() - footerHeight, getWidth(), footerHeight);
    g.setColour (juce::Colour (0xff1e2025));
    g.fillRect (footerArea);
    g.setColour (juce::Colour (0xff30343f));
    g.drawHorizontalLine (getHeight() - footerHeight, 0.0f, static_cast<float> (getWidth()));

    // 3. Tuner HUD Box in Header
    if (!tunerBoxBounds.isEmpty())
    {
        g.setColour (juce::Colour (0xff141518));
        g.fillRoundedRectangle (tunerBoxBounds.toFloat(), 5.0f);

        bool inTune = audio.tuner.getLatestResult().inTune;
        g.setColour (inTune ? juce::Colour (0xff38b000) : juce::Colour (0xff333742));
        g.drawRoundedRectangle (tunerBoxBounds.toFloat(), 5.0f, 1.2f);

        // Needle meter bar at bottom of tuner box
        auto needleArea = tunerBoxBounds.reduced (10, 0).removeFromBottom (12).withTrimmedBottom (3);
        g.setColour (juce::Colour (0xff22252d));
        g.fillRoundedRectangle (needleArea.toFloat(), 3.0f);

        float centreX = static_cast<float> (needleArea.getCentreX());
        g.setColour (juce::Colour (0xff4f5567));
        g.drawVerticalLine (static_cast<int> (centreX), static_cast<float> (needleArea.getY()), static_cast<float> (needleArea.getBottom()));

        if (std::abs (visualCents) > 0.1f)
        {
            float maxOffset = static_cast<float> (needleArea.getWidth()) * 0.46f;
            float offset = (visualCents / 50.0f) * maxOffset;
            offset = juce::jlimit (-maxOffset, maxOffset, offset);

            g.setColour (std::abs (visualCents) <= 3.0f ? juce::Colour (0xff38b000) : juce::Colour (0xff0ba4f0));
            g.fillRoundedRectangle (centreX + offset - 2.5f, static_cast<float> (needleArea.getY()) + 1.0f, 5.0f, static_cast<float> (needleArea.getHeight()) - 2.0f, 1.5f);
        }
    }

    // 4. Draw Rack Module Cards
    drawModuleCard (g, gateCardBounds, gateToggle.getToggleState());
    drawModuleCard (g, compCardBounds, compToggle.getToggleState());
    drawModuleCard (g, odCardBounds, driveToggle.getToggleState());
    drawModuleCard (g, ampCardBounds, ampToggle.getToggleState());
    drawModuleCard (g, cabCardBounds, cabToggle.getToggleState());
    drawModuleCard (g, namCardBounds, namToggle.getToggleState(), juce::Colour (0xff0ba4f0));
    drawModuleCard (g, vst3CardBounds, vst3Toggle.getToggleState());
    drawModuleCard (g, delayCardBounds, delayToggle.getToggleState());
    drawModuleCard (g, reverbCardBounds, reverbToggle.getToggleState());

    // Noise gate active clamping LED
    if (audio.gateClamping.load() && !gateCardBounds.isEmpty())
    {
        float ledX = static_cast<float> (gateCardBounds.getX()) + 138.0f;
        float ledY = static_cast<float> (gateCardBounds.getY()) + 9.0f;
        g.setColour (juce::Colour (0xffff3b5c));
        g.fillEllipse (ledX, ledY, 9.0f, 9.0f);
        g.setColour (juce::Colour (0x55ff3b5c));
        g.drawEllipse (ledX - 2.0f, ledY - 2.0f, 13.0f, 13.0f, 1.2f);
    }
}

void MainPanel::resized()
{
    auto area = getLocalBounds();

    // 1. Header layout (64px)
    auto header = area.removeFromTop (64).reduced (12, 6);

    // Tuner in exact centre of header
    const int tunerWidth = 206;
    const int tunerHeight = 50;
    tunerBoxBounds = juce::Rectangle<int> ((getWidth() - tunerWidth) / 2,
                                           header.getY() + (header.getHeight() - tunerHeight) / 2,
                                           tunerWidth, tunerHeight);

    // Tuner HUD internal layout
    auto tb = tunerBoxBounds;
    tunerNoteLabel.setBounds (tb.removeFromLeft (52));
    tb.removeFromTop (2);
    tunerCentsLabel.setBounds (tb.removeFromTop (16));
    tunerFreqLabel.setBounds (tb.removeFromTop (14));

    // Left side of header (Logo & Presets)
    auto leftArea = header.withRight (tunerBoxBounds.getX() - 8);
    appTitle.setBounds (leftArea.removeFromLeft (juce::jmin (130, leftArea.getWidth())));
    leftArea.removeFromLeft (8);
    presetLabel.setBounds (leftArea.removeFromLeft (juce::jmin (50, leftArea.getWidth())));
    leftArea.removeFromLeft (4);
    savePresetButton.setBounds (leftArea.removeFromRight (juce::jmin (52, leftArea.getWidth())));
    leftArea.removeFromRight (6);
    presetSelector.setBounds (leftArea);

    // Right side of header (Input & Audio Controls)
    auto rightArea = header.withLeft (tunerBoxBounds.getRight() + 8);
    masterMuteButton.setBounds (rightArea.removeFromRight (juce::jmin (68, rightArea.getWidth())));
    rightArea.removeFromRight (8);
    audioSettingsButton.setBounds (rightArea.removeFromRight (juce::jmin (102, rightArea.getWidth())));
    rightArea.removeFromRight (8);
    inputSourceLabel.setBounds (rightArea.removeFromLeft (juce::jmin (46, rightArea.getWidth())));
    rightArea.removeFromLeft (4);
    inputSourceSelector.setBounds (rightArea);

    // 2. Footer layout (46px)
    auto footer = area.removeFromBottom (46).reduced (14, 2);
    layoutKnob (footer.removeFromLeft (64), inputTrimSlider);
    footer.removeFromLeft (6);
    inputMeterLabel.setBounds (footer.removeFromLeft (95));

    layoutKnob (footer.removeFromRight (68), outputSlider);
    footer.removeFromRight (6);
    outputMeterLabel.setBounds (footer.removeFromRight (90));

    hardwareStatusLabel.setBounds (footer);

    // 3. Main Studio Rack Area (4 equal rows)
    auto rack = area.reduced (12, 6);
    const int totalHeight = rack.getHeight();

    // Row 1: Dynamics & Distortion Pedals (Noise Gate, Compressor, Overdrive) - 25% height
    int row1Height = (totalHeight * 25) / 100;
    auto row1 = rack.removeFromTop (row1Height);
    rack.removeFromTop (6); // gap between rows

    int col1Width = (row1.getWidth() - 16) / 3;
    gateCardBounds = row1.removeFromLeft (col1Width);
    row1.removeFromLeft (8);
    compCardBounds = row1.removeFromLeft (col1Width);
    row1.removeFromLeft (8);
    odCardBounds = row1;

    // --- Noise Gate Card ---
    {
        auto card = gateCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        gateToggle.setBounds (headerBar.removeFromLeft (126));

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 2;
        layoutKnob (knobsArea.removeFromLeft (kw), gateThresholdSlider);
        layoutKnob (knobsArea, gateReleaseSlider);
    }

    // --- Compressor Card ---
    {
        auto card = compCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        compToggle.setBounds (headerBar.removeFromLeft (126));

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 2;
        layoutKnob (knobsArea.removeFromLeft (kw), compAmountSlider);
        layoutKnob (knobsArea, compAttackSlider);
    }

    // --- Overdrive Card ---
    {
        auto card = odCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        driveToggle.setBounds (headerBar.removeFromLeft (102));
        headerBar.removeFromLeft (4);
        loadPedalButton.setBounds (headerBar.removeFromRight (90));
        headerBar.removeFromRight (4);
        driveVoicingSelector.setBounds (headerBar);

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 3;
        layoutKnob (knobsArea.removeFromLeft (kw), driveGainSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), driveToneSlider);
        layoutKnob (knobsArea, driveLevelSlider);
    }

    // Row 2: Tube Amp Head & Cabinet IR - 25% height
    int row2Height = (totalHeight * 25) / 100;
    auto row2 = rack.removeFromTop (row2Height);
    rack.removeFromTop (6); // gap between rows

    int ampWidth = ((row2.getWidth() - 8) * 68) / 100;
    ampCardBounds = row2.removeFromLeft (ampWidth);
    row2.removeFromLeft (8);
    cabCardBounds = row2;

    // --- Amp Head Card ---
    {
        auto card = ampCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        ampToggle.setBounds (headerBar.removeFromLeft (145));
        headerBar.removeFromLeft (6);
        ampVoicingSelector.setBounds (headerBar.removeFromLeft (juce::jmin (175, headerBar.getWidth())));

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 6;
        layoutKnob (knobsArea.removeFromLeft (kw), ampGainSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), ampBassSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), ampMidSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), ampTrebleSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), ampPresenceSlider);
        layoutKnob (knobsArea, ampMasterSlider);
    }

    // --- Cab / IR Card ---
    {
        auto card = cabCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        cabToggle.setBounds (headerBar);

        auto body = card.reduced (10, 3);
        cabModelSelector.setBounds (body.removeFromTop (23));
        body.removeFromTop (3);
        loadIrButton.setBounds (body.removeFromTop (23));
        body.removeFromTop (2);
        irFileNameLabel.setBounds (body.removeFromTop (16));
    }

    // Row 3: Neural Amp Modeler (NAM) & External VST3 Plugin - 25% height
    int row3Height = (totalHeight * 25) / 100;
    auto row3 = rack.removeFromTop (row3Height);
    rack.removeFromTop (6); // gap between rows

    int col3Width = (row3.getWidth() - 8) / 2;
    namCardBounds = row3.removeFromLeft (col3Width);
    row3.removeFromLeft (8);
    vst3CardBounds = row3;

    // --- Neural Amp Modeler (NAM) Card ---
    {
        auto card = namCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        namToggle.setBounds (headerBar.removeFromLeft (200));
        headerBar.removeFromLeft (4);
        getToneHuntButton.setBounds (headerBar.removeFromRight (112));
        headerBar.removeFromRight (4);
        openModelsFolderButton.setBounds (headerBar.removeFromRight (92));

        auto body = card.reduced (8, 3);
        auto knobsRight = body.removeFromRight (120);
        int kw = knobsRight.getWidth() / 2;
        layoutKnob (knobsRight.removeFromLeft (kw), namInputGainSlider);
        layoutKnob (knobsRight, namOutputGainSlider);

        body.removeFromRight (6);
        auto topRow = body.removeFromTop (24);
        unloadNamButton.setBounds (topRow.removeFromRight (64));
        topRow.removeFromRight (4);
        namModelSelector.setBounds (topRow);

        body.removeFromTop (4);
        namModelNameLabel.setBounds (body.removeFromTop (20));
    }

    // --- External VST3 Plugin Card ---
    {
        auto card = vst3CardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        vst3Toggle.setBounds (headerBar);

        auto body = card.reduced (8, 3);
        auto topBtns = body.removeFromTop (24);
        int btnW = (topBtns.getWidth() - 8) / 3;
        loadVst3Button.setBounds (topBtns.removeFromLeft (btnW));
        topBtns.removeFromLeft (4);
        openVst3UiButton.setBounds (topBtns.removeFromLeft (btnW));
        topBtns.removeFromLeft (4);
        unloadVst3Button.setBounds (topBtns);

        body.removeFromTop (4);
        vst3PluginNameLabel.setBounds (body.removeFromTop (20));
    }

    // Row 4: Stereo Delay & Lush Reverb - Remaining height
    auto row4 = rack;
    int col4Width = (row4.getWidth() - 8) / 2;
    delayCardBounds = row4.removeFromLeft (col4Width);
    row4.removeFromLeft (8);
    reverbCardBounds = row4;

    // --- Delay Card ---
    {
        auto card = delayCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        delayToggle.setBounds (headerBar.removeFromLeft (136));

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 4;
        layoutKnob (knobsArea.removeFromLeft (kw), delayTimeSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), delayFeedbackSlider);
        layoutKnob (knobsArea.removeFromLeft (kw), delayToneSlider);
        layoutKnob (knobsArea, delayMixSlider);
    }

    // --- Reverb Card ---
    {
        auto card = reverbCardBounds;
        auto headerBar = card.removeFromTop (28).reduced (8, 2);
        reverbToggle.setBounds (headerBar.removeFromLeft (116));
        headerBar.removeFromLeft (4);
        reverbFileNameLabel.setBounds (headerBar.removeFromRight (110));
        headerBar.removeFromRight (4);
        loadReverbIrButton.setBounds (headerBar);

        auto knobsArea = card.reduced (6, 3);
        int kw = knobsArea.getWidth() / 3;
        layoutKnob (knobsArea.removeFromLeft (kw), reverbDecaySlider);
        layoutKnob (knobsArea.removeFromLeft (kw), reverbDampingSlider);
        layoutKnob (knobsArea, reverbMixSlider);
    }
}
