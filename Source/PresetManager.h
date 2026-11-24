#pragma once

#include <JuceHeader.h>
#include "AudioEngine.h"

struct TonePreset
{
    juce::String name;
    bool isUserPreset = false;
    juce::String filePath;

    // Gate
    bool gateOn = false;
    float gateThreshDb = -68.0f;
    float gateReleaseMs = 60.0f;
    // Comp
    bool compOn = false;
    float compAmount = 0.4f;
    float compAttackMs = 15.0f;
    // Overdrive
    bool driveOn = true;
    int driveVoicing = 0; // TS9, Klon, Rat, HM2, Precision, BigMuff, Custom
    float driveGain = 0.5f;
    float driveTone = 0.65f;
    float driveLevel = 0.85f;
    juce::String customPedalPath;
    // Amp
    bool ampOn = true;
    int ampChannel = 2; // Lead
    float ampGain = 0.72f;
    float ampBass = 0.55f;
    float ampMiddle = 0.50f;
    float ampTreble = 0.65f;
    float ampPresence = 0.60f;
    float ampMaster = 0.75f;
    // Cab
    bool cabOn = true;
    int cabModel = 0; // V30
    juce::String customIrPath;
    // Delay
    bool delayOn = true;
    float delayTimeMs = 380.0f;
    float delayFeedback = 0.32f;
    float delayTone = 0.60f;
    float delayMix = 0.22f;
    // Reverb
    bool reverbOn = true;
    float reverbDecay = 0.55f;
    float reverbDamping = 0.35f;
    float reverbMix = 0.18f;
    juce::String customReverbPath;
    // External VST3 Plugin
    bool vst3On = false;
    int vst3Position = 1; // 0 = Pre-Amp, 1 = Post-Cab
    juce::String vst3PluginPath;
    juce::String vst3StateBase64;
    // Neural Amp Modeler
    bool namOn = false;
    juce::String namModelPath;
    float namInputGainDb = 0.0f;
    float namOutputGainDb = 0.0f;
    // Output
    float outputDb = -6.0f;

    juce::XmlElement toXml() const
    {
        juce::XmlElement xml ("TonePreset");
        xml.setAttribute ("name", name);
        xml.setAttribute ("isUserPreset", true);

        xml.setAttribute ("gateOn", gateOn);
        xml.setAttribute ("gateThreshDb", static_cast<double> (gateThreshDb));
        xml.setAttribute ("gateReleaseMs", static_cast<double> (gateReleaseMs));

        xml.setAttribute ("compOn", compOn);
        xml.setAttribute ("compAmount", static_cast<double> (compAmount));
        xml.setAttribute ("compAttackMs", static_cast<double> (compAttackMs));

        xml.setAttribute ("driveOn", driveOn);
        xml.setAttribute ("driveVoicing", driveVoicing);
        xml.setAttribute ("driveGain", static_cast<double> (driveGain));
        xml.setAttribute ("driveTone", static_cast<double> (driveTone));
        xml.setAttribute ("driveLevel", static_cast<double> (driveLevel));
        xml.setAttribute ("customPedalPath", customPedalPath);

        xml.setAttribute ("ampOn", ampOn);
        xml.setAttribute ("ampChannel", ampChannel);
        xml.setAttribute ("ampGain", static_cast<double> (ampGain));
        xml.setAttribute ("ampBass", static_cast<double> (ampBass));
        xml.setAttribute ("ampMiddle", static_cast<double> (ampMiddle));
        xml.setAttribute ("ampTreble", static_cast<double> (ampTreble));
        xml.setAttribute ("ampPresence", static_cast<double> (ampPresence));
        xml.setAttribute ("ampMaster", static_cast<double> (ampMaster));

        xml.setAttribute ("cabOn", cabOn);
        xml.setAttribute ("cabModel", cabModel);
        xml.setAttribute ("customIrPath", customIrPath);

        xml.setAttribute ("delayOn", delayOn);
        xml.setAttribute ("delayTimeMs", static_cast<double> (delayTimeMs));
        xml.setAttribute ("delayFeedback", static_cast<double> (delayFeedback));
        xml.setAttribute ("delayTone", static_cast<double> (delayTone));
        xml.setAttribute ("delayMix", static_cast<double> (delayMix));

        xml.setAttribute ("reverbOn", reverbOn);
        xml.setAttribute ("reverbDecay", static_cast<double> (reverbDecay));
        xml.setAttribute ("reverbDamping", static_cast<double> (reverbDamping));
        xml.setAttribute ("reverbMix", static_cast<double> (reverbMix));
        xml.setAttribute ("customReverbPath", customReverbPath);

        xml.setAttribute ("vst3On", vst3On);
        xml.setAttribute ("vst3Position", vst3Position);
        xml.setAttribute ("vst3PluginPath", vst3PluginPath);
        if (vst3StateBase64.isNotEmpty())
            xml.setAttribute ("vst3StateBase64", vst3StateBase64);

        xml.setAttribute ("namOn", namOn);
        xml.setAttribute ("namModelPath", namModelPath);
        xml.setAttribute ("namInputGainDb", static_cast<double> (namInputGainDb));
        xml.setAttribute ("namOutputGainDb", static_cast<double> (namOutputGainDb));

        xml.setAttribute ("outputDb", static_cast<double> (outputDb));
        return xml;
    }

    static TonePreset fromXml (const juce::XmlElement& xml, const juce::String& path = {})
    {
        TonePreset p;
        p.name = xml.getStringAttribute ("name", "Custom Profile");
        p.isUserPreset = true;
        p.filePath = path;

        p.gateOn = xml.getBoolAttribute ("gateOn", false);
        p.gateThreshDb = static_cast<float> (xml.getDoubleAttribute ("gateThreshDb", -68.0));
        p.gateReleaseMs = static_cast<float> (xml.getDoubleAttribute ("gateReleaseMs", 60.0));

        p.compOn = xml.getBoolAttribute ("compOn", false);
        p.compAmount = static_cast<float> (xml.getDoubleAttribute ("compAmount", 0.4));
        p.compAttackMs = static_cast<float> (xml.getDoubleAttribute ("compAttackMs", 15.0));

        p.driveOn = xml.getBoolAttribute ("driveOn", true);
        p.driveVoicing = xml.getIntAttribute ("driveVoicing", 0);
        p.driveGain = static_cast<float> (xml.getDoubleAttribute ("driveGain", 0.5));
        p.driveTone = static_cast<float> (xml.getDoubleAttribute ("driveTone", 0.65));
        p.driveLevel = static_cast<float> (xml.getDoubleAttribute ("driveLevel", 0.85));
        p.customPedalPath = xml.getStringAttribute ("customPedalPath");

        p.ampOn = xml.getBoolAttribute ("ampOn", true);
        p.ampChannel = xml.getIntAttribute ("ampChannel", 2);
        p.ampGain = static_cast<float> (xml.getDoubleAttribute ("ampGain", 0.72));
        p.ampBass = static_cast<float> (xml.getDoubleAttribute ("ampBass", 0.55));
        p.ampMiddle = static_cast<float> (xml.getDoubleAttribute ("ampMiddle", 0.50));
        p.ampTreble = static_cast<float> (xml.getDoubleAttribute ("ampTreble", 0.65));
        p.ampPresence = static_cast<float> (xml.getDoubleAttribute ("ampPresence", 0.60));
        p.ampMaster = static_cast<float> (xml.getDoubleAttribute ("ampMaster", 0.75));

        p.cabOn = xml.getBoolAttribute ("cabOn", true);
        p.cabModel = xml.getIntAttribute ("cabModel", 0);
        p.customIrPath = xml.getStringAttribute ("customIrPath");

        p.delayOn = xml.getBoolAttribute ("delayOn", true);
        p.delayTimeMs = static_cast<float> (xml.getDoubleAttribute ("delayTimeMs", 380.0));
        p.delayFeedback = static_cast<float> (xml.getDoubleAttribute ("delayFeedback", 0.32));
        p.delayTone = static_cast<float> (xml.getDoubleAttribute ("delayTone", 0.60));
        p.delayMix = static_cast<float> (xml.getDoubleAttribute ("delayMix", 0.22));

        p.reverbOn = xml.getBoolAttribute ("reverbOn", true);
        p.reverbDecay = static_cast<float> (xml.getDoubleAttribute ("reverbDecay", 0.55));
        p.reverbDamping = static_cast<float> (xml.getDoubleAttribute ("reverbDamping", 0.35));
        p.reverbMix = static_cast<float> (xml.getDoubleAttribute ("reverbMix", 0.18));
        p.customReverbPath = xml.getStringAttribute ("customReverbPath");

        p.vst3On = xml.getBoolAttribute ("vst3On", false);
        p.vst3Position = xml.getIntAttribute ("vst3Position", 1);
        p.vst3PluginPath = xml.getStringAttribute ("vst3PluginPath");
        p.vst3StateBase64 = xml.getStringAttribute ("vst3StateBase64");

        p.namOn = xml.getBoolAttribute ("namOn", false);
        p.namModelPath = xml.getStringAttribute ("namModelPath");
        p.namInputGainDb = static_cast<float> (xml.getDoubleAttribute ("namInputGainDb", 0.0));
        p.namOutputGainDb = static_cast<float> (xml.getDoubleAttribute ("namOutputGainDb", 0.0));

        p.outputDb = static_cast<float> (xml.getDoubleAttribute ("outputDb", -6.0));
        return p;
    }
};

class PresetManager
{
public:
    static juce::File getAppDataSubfolder (const juce::String& subfolder)
    {
        auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                        .getChildFile ("ToneForge")
                        .getChildFile (subfolder);
        if (! dir.exists())
            dir.createDirectory();
        return dir;
    }

    static juce::File getUserPresetsFolder() { return getAppDataSubfolder ("Presets"); }
    static juce::File getModelsFolder()      { return getAppDataSubfolder ("Models"); }
    static juce::File getIrsFolder()         { return getAppDataSubfolder ("IRs"); }
    static juce::File getPedalsFolder()      { return getAppDataSubfolder ("Pedals"); }

    static juce::File findProjectResourcesDir()
    {
        auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const juce::File candidates[] = {
            exeDir.getChildFile ("Resources"),
            exeDir.getParentDirectory().getChildFile ("Resources"),
            exeDir.getParentDirectory().getParentDirectory().getChildFile ("Resources"),
            juce::File ("E:/ToneForge/Resources")
        };

        for (const auto& dir : candidates)
            if (dir.isDirectory())
                return dir;

        return {};
    }

    static void deployFactoryResources()
    {
        auto modelsDir = getModelsFolder();
        auto irsDir = getIrsFolder();
        auto pedalsDir = getPedalsFolder();

        auto projectResourcesDir = findProjectResourcesDir();
        if (! projectResourcesDir.isDirectory())
            return;

        auto copyFolder = [] (const juce::File& srcDir, const juce::File& dstDir, const juce::String& pattern)
        {
            if (! srcDir.isDirectory())
                return;
            auto files = srcDir.findChildFiles (juce::File::findFiles, false, pattern);
            for (const auto& f : files)
            {
                auto dest = dstDir.getChildFile (f.getFileName());
                if (! dest.existsAsFile())
                    f.copyFileTo (dest);
            }
        };

        copyFolder (projectResourcesDir.getChildFile ("Models"), modelsDir, "*.nam;*.json");
        copyFolder (projectResourcesDir.getChildFile ("IRs"), irsDir, "*.wav");
        copyFolder (projectResourcesDir.getChildFile ("Pedals"), pedalsDir, "*.tfpedal;*.xml");
    }

    static std::vector<TonePreset> getFactoryPresets()
    {
        deployFactoryResources();

        auto modelsDir = getModelsFolder();
        auto irsDir = getIrsFolder();

        std::vector<TonePreset> list;

        // 1. Deftones White Pony Tone
        {
            TonePreset p;
            p.name = "Deftones - White Pony Tone";
            p.isUserPreset = false;
            p.gateOn = true; p.gateThreshDb = -65.0f; p.gateReleaseMs = 30.0f;
            p.compOn = false;
            p.driveOn = true; p.driveVoicing = 0; p.driveGain = 0.35f; p.driveTone = 0.70f; p.driveLevel = 0.85f;
            p.ampOn = false;
            p.namOn = true;
            p.namModelPath = modelsDir.getChildFile ("Marshall JMP-1 Rhythm Deftoned.nam").getFullPathName();
            p.cabOn = true; p.cabModel = 3; // Custom IR
            p.customIrPath = irsDir.getChildFile ("Deftones - 2000 White Pony.wav").getFullPathName();
            p.delayOn = false;
            p.reverbOn = false;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 2. Marshall JMP-1 Lead Creamy
        {
            TonePreset p;
            p.name = "Marshall JMP-1 Creamy Lead";
            p.isUserPreset = false;
            p.gateOn = false;
            p.compOn = false;
            p.driveOn = true; p.driveVoicing = 0; p.driveGain = 0.30f; p.driveTone = 0.65f; p.driveLevel = 0.85f;
            p.ampOn = false;
            p.namOn = true;
            p.namModelPath = modelsDir.getChildFile ("Marshall JMP-1 Lead Creamy.nam").getFullPathName();
            p.cabOn = true; p.cabModel = 3;
            p.customIrPath = irsDir.getChildFile ("Deftones - White Pony Marshall 4x12.wav").getFullPathName();
            p.delayOn = true; p.delayTimeMs = 420.0f; p.delayFeedback = 0.35f; p.delayTone = 0.50f; p.delayMix = 0.22f;
            p.reverbOn = true; p.reverbDecay = 0.60f; p.reverbDamping = 0.30f; p.reverbMix = 0.20f;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 3. ADA MP-1 High Gain Lead
        {
            TonePreset p;
            p.name = "ADA MP-1 Hi-Gain Lead";
            p.isUserPreset = false;
            p.gateOn = true; p.gateThreshDb = -62.0f; p.gateReleaseMs = 40.0f;
            p.compOn = false;
            p.driveOn = false;
            p.ampOn = false;
            p.namOn = true;
            p.namModelPath = modelsDir.getChildFile ("ADA MP-1 Hi Gain Lead.nam").getFullPathName();
            p.cabOn = true; p.cabModel = 3;
            p.customIrPath = irsDir.getChildFile ("Orange 4x12 V30 SM57 - Enhanced.wav").getFullPathName();
            p.delayOn = true; p.delayTimeMs = 380.0f; p.delayFeedback = 0.30f; p.delayTone = 0.60f; p.delayMix = 0.18f;
            p.reverbOn = true; p.reverbDecay = 0.55f; p.reverbDamping = 0.35f; p.reverbMix = 0.18f;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 4. Marshall 1969 Plexi JMP-50 Crunch
        {
            TonePreset p;
            p.name = "Marshall 1969 Plexi Crunch";
            p.isUserPreset = false;
            p.gateOn = false;
            p.compOn = false;
            p.driveOn = false;
            p.ampOn = false;
            p.namOn = true;
            p.namModelPath = modelsDir.getChildFile ("Marshall 1969 Plexi JMP-50 Crunch.nam").getFullPathName();
            p.cabOn = true; p.cabModel = 3;
            p.customIrPath = irsDir.getChildFile ("Orange 4x12 V30 - Studio Mix 70-30.wav").getFullPathName();
            p.delayOn = false;
            p.reverbOn = true; p.reverbDecay = 0.40f; p.reverbDamping = 0.40f; p.reverbMix = 0.15f;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 5. Deftones - Around The Fur
        {
            TonePreset p;
            p.name = "Deftones - Around The Fur";
            p.isUserPreset = false;
            p.gateOn = true; p.gateThreshDb = -65.0f; p.gateReleaseMs = 30.0f;
            p.compOn = false;
            p.driveOn = true; p.driveVoicing = 0; p.driveGain = 0.30f; p.driveTone = 0.75f; p.driveLevel = 0.85f;
            p.ampOn = false;
            p.namOn = true;
            p.namModelPath = modelsDir.getChildFile ("Marshall JMP-1 Rhythm 1.nam").getFullPathName();
            p.cabOn = true; p.cabModel = 3;
            p.customIrPath = irsDir.getChildFile ("Deftones - 1997 Around The Fur.wav").getFullPathName();
            p.delayOn = false;
            p.reverbOn = false;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 6. Modern Metal Lead (Built-in Amp)
        {
            TonePreset p;
            p.name = "Modern Metal Lead";
            p.isUserPreset = false;
            p.gateOn = false; p.gateThreshDb = -68.0f; p.gateReleaseMs = 50.0f;
            p.compOn = false;
            p.driveOn = true; p.driveVoicing = 0; p.driveGain = 0.40f; p.driveTone = 0.65f; p.driveLevel = 0.85f;
            p.ampOn = true; p.ampChannel = 2; p.ampGain = 0.75f; p.ampBass = 0.52f; p.ampMiddle = 0.48f; p.ampTreble = 0.68f; p.ampPresence = 0.62f; p.ampMaster = 0.75f;
            p.cabOn = true; p.cabModel = 0; // 4x12 V30
            p.delayOn = true; p.delayTimeMs = 420.0f; p.delayFeedback = 0.35f; p.delayTone = 0.50f; p.delayMix = 0.22f;
            p.reverbOn = true; p.reverbDecay = 0.60f; p.reverbDamping = 0.30f; p.reverbMix = 0.20f;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        // 7. Ambient Dream Clean
        {
            TonePreset p;
            p.name = "Ambient Dream Clean";
            p.isUserPreset = false;
            p.gateOn = false;
            p.compOn = true; p.compAmount = 0.55f; p.compAttackMs = 15.0f;
            p.driveOn = false;
            p.ampOn = true; p.ampChannel = 0; p.ampGain = 0.28f; p.ampBass = 0.50f; p.ampMiddle = 0.45f; p.ampTreble = 0.72f; p.ampPresence = 0.65f; p.ampMaster = 0.85f;
            p.cabOn = true; p.cabModel = 2; // 1x12 American
            p.delayOn = true; p.delayTimeMs = 520.0f; p.delayFeedback = 0.45f; p.delayTone = 0.70f; p.delayMix = 0.35f;
            p.reverbOn = true; p.reverbDecay = 0.80f; p.reverbDamping = 0.25f; p.reverbMix = 0.40f;
            p.outputDb = -6.0f;
            list.push_back (p);
        }

        return list;
    }

    static std::vector<TonePreset> loadUserPresets()
    {
        std::vector<TonePreset> list;
        auto dir = getUserPresetsFolder();
        if (! dir.isDirectory())
            return list;

        auto files = dir.findChildFiles (juce::File::findFiles, false, "*.tfpreset;*.xml");
        for (const auto& file : files)
        {
            if (auto xml = juce::parseXML (file))
            {
                if (xml->hasTagName ("TonePreset"))
                {
                    list.push_back (TonePreset::fromXml (*xml, file.getFullPathName()));
                }
            }
        }
        return list;
    }

    static bool saveUserPreset (const TonePreset& preset)
    {
        auto dir = getUserPresetsFolder();
        auto fileName = juce::File::createLegalFileName (preset.name.trim());
        if (fileName.isEmpty())
            fileName = "CustomProfile";

        auto targetFile = dir.getChildFile (fileName + ".tfpreset");
        auto xml = preset.toXml();
        return xml.writeTo (targetFile);
    }

    static bool deleteUserPreset (const juce::String& name)
    {
        auto dir = getUserPresetsFolder();
        auto fileName = juce::File::createLegalFileName (name.trim()) + ".tfpreset";
        auto targetFile = dir.getChildFile (fileName);
        if (targetFile.existsAsFile())
            return targetFile.deleteFile();
        return false;
    }

    static std::vector<TonePreset> getAllPresets()
    {
        auto all = getFactoryPresets();
        auto user = loadUserPresets();
        all.insert (all.end(), user.begin(), user.end());
        return all;
    }
};
