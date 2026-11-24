#pragma once

#include <JuceHeader.h>
#include "AudioEngine.h"
#include "PresetManager.h"
#include "CustomLookAndFeel.h"

class MainPanel final : public juce::Component,
                        private juce::Timer
{
public:
    MainPanel (ToneForgeAudio& engine, juce::AudioDeviceManager& devManager, juce::ApplicationProperties& appProperties);
    ~MainPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void setupKnob (juce::Slider& slider, const juce::String& text, double min, double max, double init, const juce::String& suffix = "");
    void setupToggle (juce::ToggleButton& btn, const juce::String& text, bool init);
    void applyPreset (int index);
    void promptSaveProfile();
    void saveProfileWithName (const juce::String& name);
    void saveCurrentPresetState();
    void refreshPresetList (const juce::String& presetNameToSelect);
    void openAudioSettings();
    void browseCustomIr();
    void browseCustomPedal();
    void browseCustomReverbIr();
    void updateTelemetryString();
    void updateCabStatusLabel();

    ToneForgeAudio& audio;
    juce::AudioDeviceManager& deviceManager;
    juce::ApplicationProperties& appProperties;
    ToneForgeLookAndFeel lookAndFeel;

    // Header Controls
    juce::Label appTitle;
    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton savePresetButton { "SAVE" };
    juce::Label inputSourceLabel;
    juce::ComboBox inputSourceSelector;
    juce::TextButton audioSettingsButton { "AUDIO SETUP" };
    juce::ToggleButton masterMuteButton { "MUTE" };

    // Tuner HUD Components
    juce::Label tunerNoteLabel;
    juce::Label tunerCentsLabel;
    juce::Label tunerFreqLabel;
    juce::ToggleButton tunerMuteButton { "TUNE MUTE" };
    float visualCents = 0.0f;

    // Rack Modules
    // 1. Noise Gate
    juce::ToggleButton gateToggle { "NOISE GATE" };
    juce::Slider gateThresholdSlider;
    juce::Slider gateReleaseSlider;

    // 2. Compressor
    juce::ToggleButton compToggle { "COMPRESSOR" };
    juce::Slider compAmountSlider;
    juce::Slider compAttackSlider;

    // 3. Overdrive
    juce::ToggleButton driveToggle { "OVERDRIVE" };
    juce::ComboBox driveVoicingSelector;
    juce::TextButton loadPedalButton { "LOAD PEDAL" };
    juce::Slider driveGainSlider;
    juce::Slider driveToneSlider;
    juce::Slider driveLevelSlider;

    // 4. Amp Head
    juce::ToggleButton ampToggle { "TUBE AMP HEAD" };
    juce::ComboBox ampVoicingSelector;
    juce::Slider ampGainSlider;
    juce::Slider ampBassSlider;
    juce::Slider ampMidSlider;
    juce::Slider ampTrebleSlider;
    juce::Slider ampPresenceSlider;
    juce::Slider ampMasterSlider;

    // 5. Cabinet / IR
    juce::ToggleButton cabToggle { "CABINET / IR" };
    juce::ComboBox cabModelSelector;
    juce::ComboBox cabIrSelector;
    juce::TextButton loadIrButton { "BROWSE IR" };
    juce::Label irFileNameLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    struct DiscoveredFile
    {
        juce::String name;
        juce::File file;
    };
    std::vector<DiscoveredFile> discoveredIrs;
    void refreshCabIrList (const juce::String& irPathToSelect = "");

    // 6. Delay
    juce::ToggleButton delayToggle { "STEREO DELAY" };
    juce::Slider delayTimeSlider;
    juce::Slider delayFeedbackSlider;
    juce::Slider delayToneSlider;
    juce::Slider delayMixSlider;

    // 7. Reverb
    juce::ToggleButton reverbToggle { "LUSH REVERB" };
    juce::TextButton loadReverbIrButton { "LOAD IR (.WAV)" };
    juce::TextButton clearReverbIrButton { "RESET" };
    juce::Label reverbFileNameLabel;
    juce::Slider reverbDecaySlider;
    juce::Slider reverbDampingSlider;
    juce::Slider reverbMixSlider;

    // 8. External VST3 Plugin
    juce::ToggleButton vst3Toggle { "EXTERNAL VST3 PLUGIN" };
    juce::ComboBox vst3PositionSelector;
    juce::ComboBox vst3PluginSelector;
    juce::TextButton openVst3UiButton { "OPEN UI" };
    juce::TextButton rescanVst3Button { "RESCAN" };
    juce::TextButton unloadVst3Button { "UNLOAD" };
    juce::Label vst3PluginNameLabel;
    std::vector<DiscoveredFile> discoveredVst3Plugins;

    void refreshVst3PluginList (const juce::String& pluginPathToSelect = "");
    void loadVst3FromFile (const juce::File& file, const juce::String& stateBase64 = {});
    void browseVst3Plugin();
    void openVst3EditorWindow();
    void unloadVst3Plugin();

    // 9. Neural Amp Modeler (NAM)
    juce::ToggleButton namToggle { "NEURAL AMP MODELER (NAM)" };
    juce::ComboBox namModelSelector;
    juce::TextButton openModelsFolderButton { "OPEN FOLDER" };
    juce::TextButton getToneHuntButton { "GET FREE MODELS" };
    juce::TextButton unloadNamButton { "UNLOAD" };
    juce::Label namModelNameLabel;
    juce::Slider namInputGainSlider;
    juce::Slider namOutputGainSlider;
    std::vector<DiscoveredFile> discoveredNamModels;

    void refreshNamModelList (const juce::String& modelPathToSelect = "");
    void browseNamModel();
    void unloadNamModel();
    void openModelsFolder();
    void openToneHuntWebsite();

    // Footer Controls
    juce::Slider inputTrimSlider;
    juce::Label inputMeterLabel;
    juce::Label hardwareStatusLabel;
    juce::Slider outputSlider;
    juce::Label outputMeterLabel;

    std::vector<TonePreset> presets;
    juce::OwnedArray<juce::Label> knobLabels;
    std::unordered_map<juce::Slider*, juce::Label*> sliderLabelMap;

    class Vst3PluginWindow final : public juce::DocumentWindow
    {
    public:
        Vst3PluginWindow (const juce::String& name, juce::AudioProcessorEditor* editor)
            : juce::DocumentWindow (name, juce::Colour (0xff191a1c), juce::DocumentWindow::allButtons)
        {
            setOpaque (true);
            setUsingNativeTitleBar (true);
            if (editor != nullptr)
            {
                setContentOwned (editor, true);
                setResizable (editor->isResizable(), false);
            }
            centreWithSize (getWidth(), getHeight());
        }

        void closeButtonPressed() override
        {
            setVisible (false);
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Vst3PluginWindow)
    };

    std::unique_ptr<Vst3PluginWindow> vst3Window;

    void layoutKnob (juce::Rectangle<int> bounds, juce::Slider& slider);
    void drawModuleCard (juce::Graphics& g, juce::Rectangle<int> bounds, bool isEnabled, const juce::Colour& accentColour = juce::Colour (0xff8049e2));

    juce::Rectangle<int> tunerBoxBounds;
    juce::Rectangle<int> gateCardBounds;
    juce::Rectangle<int> compCardBounds;
    juce::Rectangle<int> odCardBounds;
    juce::Rectangle<int> ampCardBounds;
    juce::Rectangle<int> cabCardBounds;
    juce::Rectangle<int> delayCardBounds;
    juce::Rectangle<int> reverbCardBounds;
    juce::Rectangle<int> vst3CardBounds;
    juce::Rectangle<int> namCardBounds;

    float visualInputRms = 0.0f;
    float visualOutputRms = 0.0f;
    int telemetryTickCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};
