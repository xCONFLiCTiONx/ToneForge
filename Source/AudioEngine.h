#pragma once

#include <JuceHeader.h>
#include "Tuner.h"
#include <array>
#include <cmath>
#include <atomic>
#include <memory>
#include <filesystem>

#include <NAM/get_dsp.h>
#include <NAM/dsp.h>

enum class GuitarInputRouting
{
    AutoDetect = 0, // Auto-route to whichever input has active signal
    GuitarCh2 = 1,  // Focusrite Solo Inst In (Channel 2)
    MicCh1 = 2,     // Focusrite Solo Mic In (Channel 1)
    StereoSum = 3   // Sum both channels
};

enum class AmpVoicing
{
    Clean = 0,
    Crunch = 1,
    Lead = 2
};

enum class DriveVoicing
{
    TubeScreamer = 0,         // Ibanez TS-9 (Mid-Hump Overdrive)
    KlonCentaur = 1,          // Klon Centaur (Transparent Germanium Clean Blend)
    RodentDistortion = 2,     // ProCo Rat 2 (Hard-Clipping Distortion)
    HeavyMetalChainsaw = 3,   // Boss HM-2 (Swedish Death Metal Chainsaw)
    PrecisionDrive = 4,       // Horizon Precision Drive (Modern Djent Attack)
    BigMuffFuzz = 5,          // EHX Big Muff Pi (Wall of Fuzz)
    CustomImported = 6        // User-Imported Pedal (.tfpedal, .xml, .wav, .nam)
};

enum class Vst3SlotPosition
{
    PreAmp = 0,  // Before Overdrive / Amp / NAM (ideal for gates, OD pedals, compressors, amp suites)
    PostCab = 1  // After Cabinet / NAM, before Delay & Reverb (ideal for EQ, compressors, amp suites, FX)
};

struct CustomPedalModel
{
    juce::String name = "Custom Pedal";
    juce::String filePath;
    int clipType = 0; // 0=Soft (TS), 1=Hard (Rat), 2=Germanium Asym (Klon), 3=HM-2 Chainsaw, 4=Precision Djent, 5=Extreme Fuzz
    float preFilterFreq = 720.0f;
    float gainMult = 20.0f;
    float cleanBlend = 0.0f;
    float toneBaseFreq = 2500.0f;
    bool isLoaded = false;
};

enum class CabType
{
    Vintage30_4x12 = 0,
    Greenback_2x12 = 1,
    American_1x12 = 2,
    CustomIR = 3
};

class ToneForgeAudio final : public juce::AudioIODeviceCallback
{
public:
    ToneForgeAudio()
    {
        irFormatManager.registerBasicFormats();
        pluginFormatManager.addDefaultFormats();
    }

    ~ToneForgeAudio() override
    {
        unloadNamModel();
        unloadVst3Plugin();
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        sampleRate = (device != nullptr && device->getCurrentSampleRate() > 0.0)
                         ? device->getCurrentSampleRate()
                         : 48000.0;
        maxBlockSize = juce::jmax (2048, device != nullptr ? device->getCurrentBufferSizeSamples() : 2048);

        tuner.setSampleRate (sampleRate);

        delayBufferL.assign (static_cast<size_t> (sampleRate * 2.5) + 1, 0.0f);
        delayBufferR.assign (static_cast<size_t> (sampleRate * 2.5) + 1, 0.0f);
        delayWrite = 0;

        tempInput.assign (static_cast<size_t> (maxBlockSize), 0.0f);
        tempLeft.assign (static_cast<size_t> (maxBlockSize), 0.0f);
        tempRight.assign (static_cast<size_t> (maxBlockSize), 0.0f);
        reverbWetL.assign (static_cast<size_t> (maxBlockSize), 0.0f);
        reverbWetR.assign (static_cast<size_t> (maxBlockSize), 0.0f);

        resetDsp();

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = sampleRate;
        spec.maximumBlockSize = static_cast<juce::uint32> (maxBlockSize);
        spec.numChannels = 2;

        cabConvolution.prepare (spec);
        reverbConvolution.prepare (spec);
        studioReverb.prepare (spec);

        // Prepare 4x Oversampling Engine (2^2 = 4x) for overdrive & tube amp saturation
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            1, // mono processing before stereo spatial stage
            2, // 4x oversampling factor
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true
        );
        oversampling->initProcessing (static_cast<size_t> (maxBlockSize));
        oversampling->reset();

        // Force initial filter coefficient calculation
        lastToneRate = -1.0;
        lastCabRate = -1.0;
        updateToneStackFiltersIfNeeded();
        updateCabFiltersIfNeeded();

        // Re-prepare NAM model for new sample rate / block size
        {
            const juce::ScopedLock sl (namLock);
            if (namModel != nullptr)
                namModel->Reset (sampleRate, maxBlockSize);
        }

        // Re-prepare VST3 plugin for new sample rate / block size
        {
            const juce::ScopedLock sl (pluginLock);
            if (vst3PluginInstance != nullptr)
            {
                vst3PluginInstance->prepareToPlay (sampleRate, maxBlockSize);
                int numChans = juce::jmax (2,
                                           vst3PluginInstance->getTotalNumInputChannels(),
                                           vst3PluginInstance->getTotalNumOutputChannels());
                vst3Buffer.setSize (numChans, maxBlockSize, false, true, false);
            }
            else
            {
                vst3Buffer.setSize (2, maxBlockSize, false, true, false);
            }
        }
    }

    void audioDeviceStopped() override
    {
        delayBufferL.clear();
        delayBufferR.clear();
        delayWrite = 0;
        cabConvolution.reset();
        reverbConvolution.reset();
        studioReverb.reset();
        if (oversampling != nullptr)
            oversampling->reset();

        const juce::ScopedLock sl (pluginLock);
        if (vst3PluginInstance != nullptr)
            vst3PluginInstance->releaseResources();
    }

    void updateToneStackFiltersIfNeeded()
    {
        const float bVal = ampBass.load();
        const float mVal = ampMiddle.load();
        const float tVal = ampTreble.load();
        const float pVal = ampPresence.load();

        if (lastToneRate == sampleRate
            && bVal == lastToneBass
            && mVal == lastToneMid
            && tVal == lastToneTreble
            && pVal == lastTonePresence)
        {
            return;
        }

        lastToneRate = sampleRate;
        lastToneBass = bVal;
        lastToneMid = mVal;
        lastToneTreble = tVal;
        lastTonePresence = pVal;

        // Bass: Low Shelf @ 90 Hz (-24 dB to +12 dB range)
        float bassGainDb = (bVal - 0.5f) * 36.0f - 6.0f;
        bassFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sampleRate, 90.0f, 0.707f, juce::Decibels::decibelsToGain (bassGainDb));

        // Mid: Peak Filter @ 750 Hz (-15 dB to +15 dB)
        float midGainDb = (mVal - 0.5f) * 30.0f;
        midFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sampleRate, 750.0f, 0.85f, juce::Decibels::decibelsToGain (midGainDb));

        // Treble: High Shelf @ 2.8 kHz (-18 dB to +12 dB)
        float trebGainDb = (tVal - 0.5f) * 30.0f - 3.0f;
        trebleFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 2800.0f, 0.707f, juce::Decibels::decibelsToGain (trebGainDb));

        // Presence: High Shelf @ 5.5 kHz (-12 dB to +12 dB)
        float presGainDb = (pVal - 0.5f) * 24.0f;
        presenceFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sampleRate, 5500.0f, 0.707f, juce::Decibels::decibelsToGain (presGainDb));
    }

    void updateCabFiltersIfNeeded()
    {
        const int modeInt = cabModel.load();
        if (lastCabRate == sampleRate && lastCabModel == modeInt)
            return;

        lastCabRate = sampleRate;
        lastCabModel = modeInt;

        const auto cabMode = static_cast<CabType> (modeInt);
        if (cabMode == CabType::Vintage30_4x12)
        {
            cabHpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 85.0f, 0.707f);
            cabLpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 4200.0f, 0.85f);
            cabNotchFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 1200.0f, 1.2f, 0.65f);
        }
        else if (cabMode == CabType::Greenback_2x12)
        {
            cabHpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 95.0f, 0.707f);
            cabLpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 3800.0f, 0.75f);
            cabNotchFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 450.0f, 1.0f, 1.25f);
        }
        else // American 1x12 (also fallback if CustomIR selected with no file loaded)
        {
            cabHpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 75.0f, 0.707f);
            cabLpFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, 5200.0f, 0.707f);
            cabNotchFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 600.0f, 0.9f, 0.70f);
        }
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputs,
                                           int numInputs,
                                           float* const* outputs,
                                           int numOutputs,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        if (numSamples <= 0)
            return;

        // Guard against unexpected oversized buffers
        if (static_cast<size_t> (numSamples) > tempInput.size())
        {
            tempInput.resize (static_cast<size_t> (numSamples), 0.0f);
            tempLeft.resize (static_cast<size_t> (numSamples), 0.0f);
            tempRight.resize (static_cast<size_t> (numSamples), 0.0f);
            reverbWetL.resize (static_cast<size_t> (numSamples), 0.0f);
            reverbWetR.resize (static_cast<size_t> (numSamples), 0.0f);
        }

        const float* inCh1 = (numInputs > 0 && inputs != nullptr) ? inputs[0] : nullptr;
        const float* inCh2 = (numInputs > 1 && inputs != nullptr) ? inputs[1] : nullptr;

        // Measure individual channel energies
        float sumSq1 = 0.0f;
        float sumSq2 = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float s1 = inCh1 != nullptr ? inCh1[i] : 0.0f;
            float s2 = inCh2 != nullptr ? inCh2[i] : 0.0f;
            sumSq1 += s1 * s1;
            sumSq2 += s2 * s2;
        }

        float r1 = std::sqrt (sumSq1 / static_cast<float> (numSamples));
        float r2 = std::sqrt (sumSq2 / static_cast<float> (numSamples));
        ch1Rms.store (r1);
        ch2Rms.store (r2);

        // Determine input routing
        const auto src = static_cast<GuitarInputRouting> (inputSource.load());
        bool useCh2 = false;

        if (src == GuitarInputRouting::AutoDetect)
        {
            if (inCh2 != nullptr && (r2 >= r1 || r1 < 0.0002f))
                useCh2 = true;
            else if (inCh1 != nullptr)
                useCh2 = false;
            else
                useCh2 = (inCh2 != nullptr);
        }
        else if (src == GuitarInputRouting::GuitarCh2)
        {
            useCh2 = (inCh2 != nullptr);
        }
        else if (src == GuitarInputRouting::MicCh1)
        {
            useCh2 = false;
        }

        autoActiveChannel.store (useCh2 ? 2 : 1);

        const float inTrimGain = juce::Decibels::decibelsToGain (inputTrimDb.load());
        const bool isMuted = masterMute.load();

        float sumSquaresIn = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float raw = 0.0f;
            if (src == GuitarInputRouting::StereoSum)
            {
                float s1 = inCh1 != nullptr ? inCh1[i] : 0.0f;
                float s2 = inCh2 != nullptr ? inCh2[i] : 0.0f;
                raw = 0.5f * (s1 + s2);
            }
            else if (useCh2)
            {
                raw = inCh2 != nullptr ? inCh2[i] : (inCh1 != nullptr ? inCh1[i] : 0.0f);
            }
            else
            {
                raw = inCh1 != nullptr ? inCh1[i] : (inCh2 != nullptr ? inCh2[i] : 0.0f);
            }

            raw *= inTrimGain;
            tempInput[static_cast<size_t> (i)] = raw;
            sumSquaresIn += raw * raw;
        }

        // Feed pitch detector tuner
        tuner.pushSamples (tempInput.data(), numSamples);
        float rmsIn = std::sqrt (sumSquaresIn / static_cast<float> (numSamples));
        inputRms.store (rmsIn);

        // If tuner mute or master mute active, output silence
        if (isMuted || tunerMute.load())
        {
            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs != nullptr && outputs[ch] != nullptr)
                    juce::FloatVectorOperations::clear (outputs[ch], numSamples);
            outputRms.store (0.0f);
            return;
        }

        // Snapshot DSP Parameters
        const bool gateOn = gateEnabled.load();
        const float gateThresh = juce::Decibels::decibelsToGain (gateThresholdDb.load());
        const float gateRelease = gateReleaseMs.load();

        const bool compOn = compressorEnabled.load();
        const float compAmount = compressorAmount.load();
        const float compAttack = compressorAttackMs.load();

        const bool odOn = overdriveEnabled.load();
        const auto odMode = static_cast<DriveVoicing> (overdriveVoicing.load());
        const float odDrive = overdriveGain.load();
        const float odTone = overdriveTone.load();
        const float odLevel = overdriveLevel.load();

        const bool ampOn = ampEnabled.load();
        const auto ampModel = static_cast<AmpVoicing> (ampChannel.load());
        const float aGain = ampGain.load();
        const float aMaster = ampMaster.load();

        const bool cabOn = cabinetEnabled.load();
        const auto cabMode = static_cast<CabType> (cabModel.load());

        const bool vst3On = vst3Enabled.load();
        const auto vst3Pos = static_cast<Vst3SlotPosition> (vst3Position.load());

        const bool delOn = delayEnabled.load();
        const float delMix = delayMix.load();
        const float delTime = delayTimeMs.load() / 1000.0f;
        const float delFeedback = delayFeedback.load();
        const float delTone = delayTone.load();

        const bool revOn = reverbEnabled.load();
        const float revMix = reverbMix.load();
        const float revDecay = reverbDecay.load();
        const float revDamp = reverbDamping.load();

        const float outGain = juce::Decibels::decibelsToGain (outputDb.load());

        // Update EQ & Cab filters ONLY when parameters actually changed (no per-block heap alloc)
        updateToneStackFiltersIfNeeded();
        updateCabFiltersIfNeeded();

        const auto delaySamples = juce::jlimit<size_t> (1, delayBufferL.empty() ? 1 : delayBufferL.size() - 1,
            static_cast<size_t> (juce::jmax (1.0, sampleRate * delTime)));

        bool isGatingNow = false;

        // --- STAGE 1: GATE + COMPRESSOR ---
        for (int i = 0; i < numSamples; ++i)
        {
            float x = tempInput[static_cast<size_t> (i)];

            // 1. NOISE GATE
            if (gateOn)
            {
                float absVal = std::abs (x);
                float relCoeff = static_cast<float> (1.0 / (sampleRate * (gateRelease * 0.001) + 1.0));
                gateEnv += (absVal > gateEnv ? 0.3f : relCoeff) * (absVal - gateEnv);
                if (gateEnv < gateThresh)
                {
                    isGatingNow = true;
                    float ratio = gateEnv / (gateThresh + 0.00001f);
                    float gateGain = ratio * ratio;
                    x *= gateGain;
                }
            }

            // 2. COMPRESSOR
            if (compOn && compAmount > 0.01f)
            {
                float absVal = std::abs (x);
                float attackCoeff = static_cast<float> (1.0 / (sampleRate * (compAttack * 0.001) + 1.0));
                compEnv += (absVal > compEnv ? attackCoeff : 0.002f) * (absVal - compEnv);
                const float compThresh = 0.12f;
                if (compEnv > compThresh)
                {
                    float compressed = compThresh + (compEnv - compThresh) / (1.0f + compAmount * 5.0f);
                    x *= compressed / juce::jmax (compEnv, 0.0001f);
                }
                x *= (1.0f + compAmount * 1.6f);
            }

            tempLeft[static_cast<size_t> (i)] = x;
            tempRight[static_cast<size_t> (i)] = x;
        }

        gateClamping.store (isGatingNow);

        // --- OPTIONAL PRE-AMP VST3 SLOT ---
        if (vst3On && vst3Pos == Vst3SlotPosition::PreAmp && hasVst3Loaded.load())
        {
            processVst3Slot (numSamples);
        }

        // --- STAGE 2: PRE-GAIN HIGH-PASS TIGHTENER + OVERDRIVE + TUBE AMP SATURATION (4x OVERSAMPLED) ---
        if (odOn || ampOn)
        {
            float preCutFreq = 120.0f;
            if (ampModel == AmpVoicing::Crunch) preCutFreq = 250.0f;
            else if (ampModel == AmpVoicing::Lead) preCutFreq = 380.0f;

            if (odOn)
            {
                if (odMode == DriveVoicing::TubeScreamer) preCutFreq = 650.0f;
                else if (odMode == DriveVoicing::PrecisionDrive) preCutFreq = 750.0f;
                else if (odMode == DriveVoicing::HeavyMetalChainsaw) preCutFreq = 450.0f;
                else if (odMode == DriveVoicing::RodentDistortion) preCutFreq = 350.0f;
                else if (odMode == DriveVoicing::CustomImported && customPedal.isLoaded) preCutFreq = customPedal.preFilterFreq;
            }

            auto processNonLinearSamples = [&] (float* data, int count, double procRate)
            {
                const float preCutCoeff = std::exp (-2.0f * 3.14159265f * preCutFreq / static_cast<float> (procRate));

                for (int i = 0; i < count; ++i)
                {
                    float x = data[i];

                    // Pre-gain bass cut (High-Pass Filter)
                    preHpState = preHpState * preCutCoeff + x * (1.0f - preCutCoeff);
                    float tightInput = x - preHpState * 0.92f;

                    // 3. OVERDRIVE PEDAL
                    if (odOn)
                    {
                        float pedalIn = tightInput;
                        float driven = 0.0f;
                        float clipped = 0.0f;
                        float cleanBlend = 0.0f;

                        if (odMode == DriveVoicing::TubeScreamer)
                        {
                            driven = pedalIn * (1.0f + odDrive * 12.0f);
                            clipped = std::tanh (driven);
                        }
                        else if (odMode == DriveVoicing::KlonCentaur)
                        {
                            driven = pedalIn * (1.0f + odDrive * 14.0f);
                            clipped = driven > 0.0f ? std::tanh (driven * 1.25f) * 0.82f : std::tanh (driven * 0.92f);
                            cleanBlend = juce::jlimit (0.0f, 0.40f, (1.0f - odDrive) * 0.50f);
                        }
                        else if (odMode == DriveVoicing::RodentDistortion)
                        {
                            driven = pedalIn * (1.0f + odDrive * 32.0f);
                            clipped = juce::jlimit (-0.78f, 0.78f, driven);
                        }
                        else if (odMode == DriveVoicing::HeavyMetalChainsaw)
                        {
                            driven = pedalIn * (1.0f + odDrive * 40.0f);
                            clipped = juce::jlimit (-0.75f, 0.75f, std::tanh (driven * 1.6f));
                        }
                        else if (odMode == DriveVoicing::PrecisionDrive)
                        {
                            driven = pedalIn * (1.0f + odDrive * 28.0f);
                            clipped = std::tanh (driven * 1.5f);
                        }
                        else if (odMode == DriveVoicing::BigMuffFuzz)
                        {
                            driven = pedalIn * (1.0f + odDrive * 50.0f);
                            clipped = (driven > 0.0f ? 1.0f : -1.0f) * (1.0f - std::exp (-std::abs (driven)));
                        }
                        else // Custom Imported Pedal
                        {
                            driven = pedalIn * (1.0f + odDrive * customPedal.gainMult);
                            if (customPedal.clipType == 1)
                                clipped = juce::jlimit (-0.80f, 0.80f, driven);
                            else if (customPedal.clipType == 2)
                                clipped = driven > 0.0f ? std::tanh (driven * 1.25f) * 0.82f : std::tanh (driven * 0.92f);
                            else if (customPedal.clipType == 3)
                                clipped = juce::jlimit (-0.75f, 0.75f, std::tanh (driven * 1.5f));
                            else if (customPedal.clipType == 5)
                                clipped = (driven > 0.0f ? 1.0f : -1.0f) * (1.0f - std::exp (-std::abs (driven)));
                            else
                                clipped = std::tanh (driven);

                            cleanBlend = customPedal.cleanBlend;
                        }

                        // Tone shaping post-filter (rate-compensated)
                        float toneFreq = 800.0f + odTone * 5500.0f;
                        float toneCoeff = 1.0f - std::exp (-2.0f * 3.14159265f * toneFreq / static_cast<float> (procRate));
                        odPostFilterState = odPostFilterState + toneCoeff * (clipped - odPostFilterState);

                        float wetSignal = odPostFilterState * (0.75f + odLevel * 0.65f);
                        tightInput = (pedalIn * cleanBlend) + (wetSignal * (1.0f - cleanBlend));
                    }

                    // 4. TUBE AMP HEAD STAGE
                    if (ampOn)
                    {
                        float ampIn = tightInput;

                        if (ampModel == AmpVoicing::Clean)
                        {
                            float drive1 = 1.0f + aGain * 6.0f;
                            float tube1 = triodeStage (ampIn, drive1, 0.08f);
                            x = tube1 * (aMaster * 1.4f);
                        }
                        else if (ampModel == AmpVoicing::Crunch)
                        {
                            float drive1 = 1.0f + aGain * 14.0f;
                            float t1 = triodeStage (ampIn, drive1, 0.12f);
                            ampStage1State = ampStage1State * 0.60f + t1 * 0.40f;
                            float t2 = triodeStage (ampStage1State, 2.2f, 0.05f);
                            x = t2 * (aMaster * 1.3f);
                        }
                        else // Lead High Gain 5150 / Rectifier
                        {
                            float drive1 = 1.0f + aGain * 32.0f;
                            float t1 = triodeStage (ampIn, drive1 * 0.5f, 0.15f);
                            ampStage1State = ampStage1State * 0.50f + t1 * 0.50f;

                            float t2 = triodeStage (ampStage1State, 3.2f, 0.08f);
                            ampStage2State = ampStage2State * 0.55f + t2 * 0.45f;

                            float t3 = triodeStage (ampStage2State, 2.4f, 0.02f);
                            x = t3 * (aMaster * 1.25f);
                        }
                    }
                    else
                    {
                        x = tightInput;
                    }

                    data[i] = x;
                }
            };

            if (oversampling != nullptr && numSamples <= maxBlockSize)
            {
                float* monoPtr[1] = { tempLeft.data() };
                juce::dsp::AudioBlock<float> monoBlock (monoPtr, 1, static_cast<size_t> (numSamples));
                auto osBlock = oversampling->processSamplesUp (monoBlock);
                processNonLinearSamples (osBlock.getChannelPointer (0),
                                         static_cast<int> (osBlock.getNumSamples()),
                                         sampleRate * oversampling->getOversamplingFactor());
                oversampling->processSamplesDown (monoBlock);
            }
            else
            {
                processNonLinearSamples (tempLeft.data(), numSamples, sampleRate);
            }
        }

        // --- STAGE 3: 3-BAND EQ + PRESENCE TONE STACK ---
        if (ampOn)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float x = tempLeft[static_cast<size_t> (i)];
                x = bassFilter.processSample (x);
                x = midFilter.processSample (x);
                x = trebleFilter.processSample (x);
                x = presenceFilter.processSample (x);
                tempLeft[static_cast<size_t> (i)] = x;
            }
        }

        // --- STAGE 4: NEURAL AMP MODELER (NAM) ENGINE ---
        const bool isNamActive = namEnabled.load();
        if (isNamActive && hasNamLoaded.load())
        {
            const juce::ScopedTryLock tryLock (namLock);
            if (tryLock.isLocked() && namModel != nullptr)
            {
                float namInGain = juce::Decibels::decibelsToGain (namInputGainDb.load());
                float namOutGain = juce::Decibels::decibelsToGain (namOutputGainDb.load());

                for (int i = 0; i < numSamples; ++i)
                    tempLeft[static_cast<size_t> (i)] *= namInGain;

                float* inPtrs[1] = { tempLeft.data() };
                float* outPtrs[1] = { tempRight.data() };

                namModel->process (inPtrs, outPtrs, numSamples);

                for (int i = 0; i < numSamples; ++i)
                {
                    float s = tempRight[static_cast<size_t> (i)] * namOutGain;
                    if (! std::isfinite (s))
                        s = 0.0f;
                    tempLeft[static_cast<size_t> (i)] = s;
                    tempRight[static_cast<size_t> (i)] = s;
                }
            }
        }

        // --- STAGE 5: CABINET / SPEAKER SIMULATION ---
        if (cabOn)
        {
            if (cabMode == CabType::CustomIR && hasCustomIrLoaded.load())
            {
                juce::FloatVectorOperations::copy (tempRight.data(), tempLeft.data(), numSamples);
                tempPointers[0] = tempLeft.data();
                tempPointers[1] = tempRight.data();
                juce::dsp::AudioBlock<float> block (tempPointers, 2, static_cast<size_t> (numSamples));
                juce::dsp::ProcessContextReplacing<float> context (block);
                cabConvolution.process (context);
            }
            else
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float x = tempLeft[static_cast<size_t> (i)];
                    x = cabHpFilter.processSample (x);
                    x = cabLpFilter.processSample (x);
                    x = cabNotchFilter.processSample (x);
                    tempLeft[static_cast<size_t> (i)] = x;
                    tempRight[static_cast<size_t> (i)] = x;
                }
            }
        }
        else
        {
            juce::FloatVectorOperations::copy (tempRight.data(), tempLeft.data(), numSamples);
        }

        // --- STAGE 6: POST-CAB EXTERNAL VST3 PLUGIN HOST SLOT ---
        if (vst3On && vst3Pos == Vst3SlotPosition::PostCab && hasVst3Loaded.load())
        {
            processVst3Slot (numSamples);
        }

        // --- STAGE 7A: STEREO PING-PONG TAPE DELAY ---
        if (delOn && delMix > 0.001f && ! delayBufferL.empty())
        {
            const float damp = 0.30f + delTone * 0.60f;
            for (int i = 0; i < numSamples; ++i)
            {
                float l = tempLeft[static_cast<size_t> (i)];
                float r = tempRight[static_cast<size_t> (i)];

                const size_t readL = (delayWrite + delayBufferL.size() - delaySamples) % delayBufferL.size();
                const size_t offsetR = juce::jmax<size_t> (1, static_cast<size_t> (delaySamples * 0.85f));
                const size_t readR = (delayWrite + delayBufferR.size() - offsetR) % delayBufferR.size();

                float delayedL = delayBufferL[readL];
                float delayedR = delayBufferR[readR];

                delayDampL = delayDampL * (1.0f - damp) + delayedL * damp;
                delayDampR = delayDampR * (1.0f - damp) + delayedR * damp;

                delayBufferL[delayWrite] = l + delayDampR * delFeedback;
                delayBufferR[delayWrite] = r + delayDampL * delFeedback;

                tempLeft[static_cast<size_t> (i)] = l * (1.0f - delMix * 0.65f) + delayDampL * delMix;
                tempRight[static_cast<size_t> (i)] = r * (1.0f - delMix * 0.65f) + delayDampR * delMix;

                delayWrite = (delayWrite + 1) % delayBufferL.size();
            }
        }

        // --- STAGE 7B: STUDIO REVERB (BUILT-IN FREEVERB OR DEDICATED CONVOLUTION IR) ---
        if (revOn && revMix > 0.001f)
        {
            if (hasCustomReverbLoaded.load())
            {
                juce::FloatVectorOperations::copy (reverbWetL.data(), tempLeft.data(), numSamples);
                juce::FloatVectorOperations::copy (reverbWetR.data(), tempRight.data(), numSamples);
                reverbPointers[0] = reverbWetL.data();
                reverbPointers[1] = reverbWetR.data();
                juce::dsp::AudioBlock<float> wetBlock (reverbPointers, 2, static_cast<size_t> (numSamples));
                juce::dsp::ProcessContextReplacing<float> wetCtx (wetBlock);
                reverbConvolution.process (wetCtx);

                const float dryAmt = 1.0f - revMix * 0.5f;
                for (int i = 0; i < numSamples; ++i)
                {
                    tempLeft[static_cast<size_t> (i)] = tempLeft[static_cast<size_t> (i)] * dryAmt + reverbWetL[static_cast<size_t> (i)] * revMix;
                    tempRight[static_cast<size_t> (i)] = tempRight[static_cast<size_t> (i)] * dryAmt + reverbWetR[static_cast<size_t> (i)] * revMix;
                }
            }
            else
            {
                juce::dsp::Reverb::Parameters revParams;
                revParams.roomSize = 0.15f + revDecay * 0.80f;
                revParams.damping = revDamp;
                revParams.wetLevel = revMix;
                revParams.dryLevel = 1.0f - revMix * 0.35f;
                revParams.width = 1.0f;
                revParams.freezeMode = 0.0f;
                studioReverb.setParameters (revParams);

                tempPointers[0] = tempLeft.data();
                tempPointers[1] = tempRight.data();
                juce::dsp::AudioBlock<float> revBlock (tempPointers, 2, static_cast<size_t> (numSamples));
                juce::dsp::ProcessContextReplacing<float> revCtx (revBlock);
                studioReverb.process (revCtx);
            }
        }

        // --- STAGE 8: MASTER OUTPUT GAIN, SANITIZER & SAFETY LIMITER ---
        float sumSquaresOut = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float l = tempLeft[static_cast<size_t> (i)];
            float r = tempRight[static_cast<size_t> (i)];

            if (! std::isfinite (l)) l = 0.0f;
            if (! std::isfinite (r)) r = 0.0f;

            l = softLimit (l * outGain);
            r = softLimit (r * outGain);

            sumSquaresOut += 0.5f * (l * l + r * r);

            if (numOutputs > 0 && outputs != nullptr && outputs[0] != nullptr)
                outputs[0][i] = l;
            if (numOutputs > 1 && outputs != nullptr && outputs[1] != nullptr)
                outputs[1][i] = r;
        }

        outputRms.store (std::sqrt (sumSquaresOut / static_cast<float> (numSamples)));
    }

    // ==================== NAM MODEL MANAGEMENT ====================

    bool loadNamModel (const juce::File& file, juce::String& errorMsg)
    {
        if (! file.existsAsFile())
        {
            errorMsg = "File does not exist: " + file.getFullPathName();
            return false;
        }

        try
        {
            std::filesystem::path stdPath (file.getFullPathName().toWideCharPointer());
            auto newModel = nam::get_dsp (stdPath);

            if (newModel == nullptr)
            {
                errorMsg = "Could not parse or initialize NAM model:\n" + file.getFileName();
                return false;
            }

            // Prepare & prewarm model for current sample rate and block size outside audio thread
            newModel->Reset (sampleRate, maxBlockSize);

            {
                const juce::ScopedLock sl (namLock);
                namModel = std::move (newModel);

                namModelName = file.getFileNameWithoutExtension();
                double rate = namModel->GetExpectedSampleRate();
                if (rate > 0.0)
                    namModelName += " (" + juce::String (static_cast<int> (rate)) + " Hz)";

                namModelPath = file.getFullPathName();
                hasNamLoaded.store (true);
                namEnabled.store (true);
            }

            return true;
        }
        catch (const std::exception& e)
        {
            errorMsg = "NAM Loading Error:\n" + juce::String (e.what());
            return false;
        }
        catch (...)
        {
            errorMsg = "An unknown error occurred while loading NAM model.";
            return false;
        }
    }

    void unloadNamModel()
    {
        const juce::ScopedLock sl (namLock);
        namModel.reset();
        namModelName = "No NAM Model Loaded";
        namModelPath = "";
        hasNamLoaded.store (false);
        namEnabled.store (false);
    }

    bool isNamModelLoaded() const { return hasNamLoaded.load(); }

    // ==================== VST3 PLUGIN MANAGEMENT ====================

    static juce::File resolveVst3BundlePath (const juce::File& input)
    {
        // If user selected the inner .vst3 DLL inside *.vst3/Contents/x86_64-win/, walk up to the .vst3 bundle root
        auto cur = input;
        for (int depth = 0; depth < 4 && cur.exists(); ++depth)
        {
            if (cur.isDirectory() && cur.hasFileExtension (".vst3"))
                return cur;
            auto parent = cur.getParentDirectory();
            if (parent == cur)
                break;
            cur = parent;
        }
        return input;
    }

    bool loadVst3Plugin (const juce::File& rawFile, juce::String& errorMsg)
    {
        auto file = resolveVst3BundlePath (rawFile);
        if (! file.exists())
        {
            errorMsg = "File does not exist: " + file.getFullPathName();
            return false;
        }

        juce::OwnedArray<juce::PluginDescription> typesFound;
        for (auto* format : pluginFormatManager.getFormats())
        {
            if (format->getName().equalsIgnoreCase ("VST3"))
            {
                format->findAllTypesForFile (typesFound, file.getFullPathName());
                break;
            }
        }

        if (typesFound.isEmpty())
        {
            errorMsg = "No valid 64-bit VST3 plugin found in:\n" + file.getFullPathName();
            return false;
        }

        juce::String createErr;
        auto newInstance = pluginFormatManager.createPluginInstance (*typesFound[0], sampleRate, maxBlockSize, createErr);

        if (newInstance == nullptr)
        {
            errorMsg = "Failed to instantiate VST3 plugin:\n" + createErr;
            return false;
        }

        // Configure stereo bus layout cleanly (required by many modern VST3s like TONE3000, Gateway, Focusrite Red)
        newInstance->enableAllBuses();
        juce::AudioProcessor::BusesLayout stereoLayout;
        stereoLayout.inputBuses.add (juce::AudioChannelSet::stereo());
        stereoLayout.outputBuses.add (juce::AudioChannelSet::stereo());
        if (newInstance->checkBusesLayoutSupported (stereoLayout))
        {
            newInstance->setBusesLayout (stereoLayout);
        }
        else
        {
            // Fallback: mono-in / stereo-out (common for guitar amp sims)
            juce::AudioProcessor::BusesLayout monoInStereoOut;
            monoInStereoOut.inputBuses.add (juce::AudioChannelSet::mono());
            monoInStereoOut.outputBuses.add (juce::AudioChannelSet::stereo());
            if (newInstance->checkBusesLayoutSupported (monoInStereoOut))
                newInstance->setBusesLayout (monoInStereoOut);
        }

        newInstance->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
        newInstance->prepareToPlay (sampleRate, maxBlockSize);

        const int numChans = juce::jmax (2,
                                         newInstance->getTotalNumInputChannels(),
                                         newInstance->getTotalNumOutputChannels());

        {
            const juce::ScopedLock sl (pluginLock);
            if (vst3PluginInstance != nullptr)
            {
                vst3PluginInstance->releaseResources();
                vst3PluginInstance.reset();
            }

            vst3PluginInstance = std::move (newInstance);
            vst3Buffer.setSize (numChans, maxBlockSize, false, true, false);

            vst3PluginName = vst3PluginInstance->getName();
            auto vendor = vst3PluginInstance->getPluginDescription().manufacturerName;
            if (vendor.isNotEmpty())
                vst3PluginName += " (" + vendor + ")";

            vst3PluginPath = file.getFullPathName();
            hasVst3Loaded.store (true);
            vst3Enabled.store (true);
        }

        return true;
    }

    void unloadVst3Plugin()
    {
        const juce::ScopedLock sl (pluginLock);
        if (vst3PluginInstance != nullptr)
        {
            vst3PluginInstance->releaseResources();
            vst3PluginInstance.reset();
        }
        vst3PluginName = "No VST3 Plugin Loaded";
        vst3PluginPath = "";
        hasVst3Loaded.store (false);
        vst3Enabled.store (false);
    }

    bool isVst3PluginLoaded() const { return hasVst3Loaded.load(); }

    juce::AudioProcessorEditor* createVst3PluginEditor()
    {
        const juce::ScopedLock sl (pluginLock);
        if (vst3PluginInstance != nullptr)
        {
            if (auto* existing = vst3PluginInstance->getActiveEditor())
                return existing;
            if (vst3PluginInstance->hasEditor())
                return vst3PluginInstance->createEditor();
        }
        return nullptr;
    }

    juce::String getVst3StateBase64()
    {
        const juce::ScopedLock sl (pluginLock);
        if (vst3PluginInstance != nullptr)
        {
            juce::MemoryBlock mb;
            vst3PluginInstance->getStateInformation (mb);
            if (mb.getSize() > 0)
                return mb.toBase64Encoding();
        }
        return {};
    }

    void setVst3StateBase64 (const juce::String& base64State)
    {
        if (base64State.isEmpty())
            return;

        const juce::ScopedLock sl (pluginLock);
        if (vst3PluginInstance != nullptr)
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (base64State) && mb.getSize() > 0)
                vst3PluginInstance->setStateInformation (mb.getData(), static_cast<int> (mb.getSize()));
        }
    }

    // ==================== CABINET IR, PEDAL & REVERB IR ====================

    bool loadCustomIrFile (const juce::File& irFile)
    {
        if (! irFile.existsAsFile())
            return false;

        try
        {
            cabConvolution.loadImpulseResponse (irFile,
                                                juce::dsp::Convolution::Stereo::yes,
                                                juce::dsp::Convolution::Trim::yes,
                                                0,
                                                juce::dsp::Convolution::Normalise::yes);
            hasCustomIrLoaded.store (true);
            customIrName = irFile.getFileName();
            customIrPath = irFile.getFullPathName();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool loadCustomPedalFile (const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;

        auto ext = file.getFileExtension().toLowerCase();
        if (ext == ".nam" || ext == ".json")
        {
            juce::String err;
            if (loadNamModel (file, err))
            {
                customPedal.name = file.getFileNameWithoutExtension();
                customPedal.filePath = file.getFullPathName();
                customPedal.isLoaded = true;

                customPedalName = customPedal.name;
                customPedalPath = file.getFullPathName();
                overdriveVoicing.store (static_cast<int> (DriveVoicing::CustomImported));
                return true;
            }
        }
        else if (ext == ".tfpedal" || ext == ".xml")
        {
            if (auto xml = juce::parseXML (file))
            {
                if (xml->hasTagName ("TonePedal") || xml->hasTagName ("TonePreset") || xml->hasTagName ("Pedal"))
                {
                    customPedal.name = xml->getStringAttribute ("name", file.getFileNameWithoutExtension());
                    customPedal.filePath = file.getFullPathName();
                    customPedal.clipType = xml->getIntAttribute ("clipType", 0);
                    customPedal.preFilterFreq = static_cast<float> (xml->getDoubleAttribute ("preFilterFreq", 720.0));
                    customPedal.gainMult = static_cast<float> (xml->getDoubleAttribute ("gainMult", 20.0));
                    customPedal.cleanBlend = static_cast<float> (xml->getDoubleAttribute ("cleanBlend", 0.0));
                    customPedal.toneBaseFreq = static_cast<float> (xml->getDoubleAttribute ("toneBaseFreq", 2500.0));
                    customPedal.isLoaded = true;

                    customPedalName = customPedal.name;
                    customPedalPath = file.getFullPathName();
                    overdriveVoicing.store (static_cast<int> (DriveVoicing::CustomImported));
                    return true;
                }
            }
        }
        else if (ext == ".wav")
        {
            customPedal.name = file.getFileNameWithoutExtension();
            customPedal.filePath = file.getFullPathName();
            customPedal.clipType = 1;
            customPedal.preFilterFreq = 850.0f;
            customPedal.gainMult = 24.0f;
            customPedal.cleanBlend = 0.15f;
            customPedal.isLoaded = true;

            customPedalName = customPedal.name;
            customPedalPath = file.getFullPathName();
            overdriveVoicing.store (static_cast<int> (DriveVoicing::CustomImported));
            return true;
        }

        return false;
    }

    bool loadCustomReverbIrFile (const juce::File& irFile)
    {
        if (! irFile.existsAsFile())
            return false;

        try
        {
            reverbConvolution.loadImpulseResponse (irFile,
                                                   juce::dsp::Convolution::Stereo::yes,
                                                   juce::dsp::Convolution::Trim::yes,
                                                   0,
                                                   juce::dsp::Convolution::Normalise::yes);
            hasCustomReverbLoaded.store (true);
            customReverbName = irFile.getFileName();
            customReverbPath = irFile.getFullPathName();
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void unloadCustomReverbIr()
    {
        hasCustomReverbLoaded.store (false);
        customReverbName = "None";
        customReverbPath = "";
    }

    void resetDsp()
    {
        gateEnv = 0.0f;
        compEnv = 0.0f;
        odPostFilterState = 0.0f;
        ampStage1State = 0.0f;
        ampStage2State = 0.0f;
        preHpState = 0.0f;
        delayDampL = 0.0f;
        delayDampR = 0.0f;

        bassFilter.reset();
        midFilter.reset();
        trebleFilter.reset();
        presenceFilter.reset();
        cabHpFilter.reset();
        cabLpFilter.reset();
        cabNotchFilter.reset();
        studioReverb.reset();
    }

    // Parameters
    std::atomic<int> inputSource { static_cast<int> (GuitarInputRouting::AutoDetect) };
    std::atomic<int> autoActiveChannel { 2 };
    std::atomic<float> inputTrimDb { 0.0f };
    std::atomic<bool> masterMute { false };
    std::atomic<bool> tunerMute { false };

    // Noise Gate
    std::atomic<bool> gateEnabled { false };
    std::atomic<float> gateThresholdDb { -68.0f };
    std::atomic<float> gateReleaseMs { 60.0f };
    std::atomic<bool> gateClamping { false };

    // Compressor
    std::atomic<bool> compressorEnabled { false };
    std::atomic<float> compressorAmount { 0.4f };
    std::atomic<float> compressorAttackMs { 15.0f };

    // Overdrive
    std::atomic<bool> overdriveEnabled { true };
    std::atomic<int> overdriveVoicing { static_cast<int> (DriveVoicing::TubeScreamer) };
    std::atomic<float> overdriveGain { 0.5f };
    std::atomic<float> overdriveTone { 0.65f };
    std::atomic<float> overdriveLevel { 0.85f };

    // Amp
    std::atomic<bool> ampEnabled { true };
    std::atomic<int> ampChannel { static_cast<int> (AmpVoicing::Lead) };
    std::atomic<float> ampGain { 0.72f };
    std::atomic<float> ampBass { 0.55f };
    std::atomic<float> ampMiddle { 0.50f };
    std::atomic<float> ampTreble { 0.65f };
    std::atomic<float> ampPresence { 0.60f };
    std::atomic<float> ampMaster { 0.75f };

    // Cabinet
    std::atomic<bool> cabinetEnabled { true };
    std::atomic<int> cabModel { static_cast<int> (CabType::Vintage30_4x12) };

    // Neural Amp Modeler (NAM)
    std::atomic<bool> namEnabled { false };
    std::atomic<bool> hasNamLoaded { false };
    std::atomic<float> namInputGainDb { 0.0f };
    std::atomic<float> namOutputGainDb { 0.0f };
    juce::String namModelName = "No NAM Model Loaded";
    juce::String namModelPath;

    // Delay
    std::atomic<bool> delayEnabled { true };
    std::atomic<float> delayTimeMs { 380.0f };
    std::atomic<float> delayFeedback { 0.32f };
    std::atomic<float> delayTone { 0.60f };
    std::atomic<float> delayMix { 0.22f };

    // Reverb
    std::atomic<bool> reverbEnabled { true };
    std::atomic<float> reverbDecay { 0.55f };
    std::atomic<float> reverbDamping { 0.35f };
    std::atomic<float> reverbMix { 0.18f };

    // External VST3 Plugin
    std::atomic<bool> vst3Enabled { false };
    std::atomic<bool> hasVst3Loaded { false };
    std::atomic<int> vst3Position { static_cast<int> (Vst3SlotPosition::PostCab) };
    juce::String vst3PluginName = "No VST3 Plugin Loaded";
    juce::String vst3PluginPath;

    // Output (sensible default: -6 dB, protected by soft limiter)
    std::atomic<float> outputDb { -6.0f };

    // Telemetry
    std::atomic<float> ch1Rms { 0.0f };
    std::atomic<float> ch2Rms { 0.0f };
    std::atomic<float> inputRms { 0.0f };
    std::atomic<float> outputRms { 0.0f };

    GuitarTuner tuner;
    std::atomic<bool> hasCustomIrLoaded { false };
    juce::String customIrName = "None";
    juce::String customIrPath;

    CustomPedalModel customPedal;
    juce::String customPedalName = "None";
    juce::String customPedalPath;

    std::atomic<bool> hasCustomReverbLoaded { false };
    juce::String customReverbName = "None";
    juce::String customReverbPath;

private:
    void processVst3Slot (int numSamples)
    {
        const juce::ScopedTryLock tryLock (pluginLock);
        if (! tryLock.isLocked() || vst3PluginInstance == nullptr)
            return;

        const int totalIn = vst3PluginInstance->getTotalNumInputChannels();
        const int totalOut = vst3PluginInstance->getTotalNumOutputChannels();
        const int numChans = juce::jmax (2, totalIn, totalOut);

        if (vst3Buffer.getNumChannels() < numChans || vst3Buffer.getNumSamples() < numSamples)
            vst3Buffer.setSize (numChans, numSamples, false, false, true);

        vst3Buffer.clear (0, numSamples);
        if (totalIn >= 1)
            vst3Buffer.copyFrom (0, 0, tempLeft.data(), numSamples);
        if (totalIn >= 2)
            vst3Buffer.copyFrom (1, 0, tempRight.data(), numSamples);

        vst3MidiBuffer.clear();
        vst3PluginInstance->processBlock (vst3Buffer, vst3MidiBuffer);

        if (totalOut >= 1)
            juce::FloatVectorOperations::copy (tempLeft.data(), vst3Buffer.getReadPointer (0), numSamples);
        if (totalOut >= 2)
            juce::FloatVectorOperations::copy (tempRight.data(), vst3Buffer.getReadPointer (1), numSamples);
        else if (totalOut == 1)
            juce::FloatVectorOperations::copy (tempRight.data(), vst3Buffer.getReadPointer (0), numSamples);
    }

    static inline float triodeStage (float x, float drive, float bias)
    {
        float driven = (x + bias) * drive;
        float sat = driven > 0.0f ? std::tanh (driven * 1.15f) / 1.15f : std::tanh (driven * 0.85f) / 0.85f;
        return sat - (std::tanh (bias * drive * 1.15f) / 1.15f);
    }

    static float softLimit (float x)
    {
        if (x > 0.95f)
            return 0.95f + 0.05f * std::tanh ((x - 0.95f) / 0.05f);
        if (x < -0.95f)
            return -0.95f + 0.05f * std::tanh ((x + 0.95f) / 0.05f);
        return x;
    }

    double sampleRate = 48000.0;
    int maxBlockSize = 2048;

    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    size_t delayWrite = 0;

    std::vector<float> tempInput;
    std::vector<float> tempLeft;
    std::vector<float> tempRight;
    std::vector<float> reverbWetL;
    std::vector<float> reverbWetR;
    float* tempPointers[2] = { nullptr, nullptr };
    float* reverbPointers[2] = { nullptr, nullptr };

    juce::dsp::Convolution cabConvolution;
    juce::dsp::Convolution reverbConvolution;
    juce::dsp::Reverb studioReverb;
    juce::AudioFormatManager irFormatManager;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // 3-Band EQ + Presence
    juce::dsp::IIR::Filter<float> bassFilter;
    juce::dsp::IIR::Filter<float> midFilter;
    juce::dsp::IIR::Filter<float> trebleFilter;
    juce::dsp::IIR::Filter<float> presenceFilter;
    double lastToneRate = -1.0;
    float lastToneBass = -1.0f;
    float lastToneMid = -1.0f;
    float lastToneTreble = -1.0f;
    float lastTonePresence = -1.0f;

    // Cabinet Filters
    juce::dsp::IIR::Filter<float> cabHpFilter;
    juce::dsp::IIR::Filter<float> cabLpFilter;
    juce::dsp::IIR::Filter<float> cabNotchFilter;
    double lastCabRate = -1.0;
    int lastCabModel = -1;

    // NAM Engine
    std::unique_ptr<nam::DSP> namModel;
    juce::CriticalSection namLock;

    // VST3 Hosting
    juce::AudioPluginFormatManager pluginFormatManager;
    std::unique_ptr<juce::AudioPluginInstance> vst3PluginInstance;
    juce::CriticalSection pluginLock;
    juce::AudioBuffer<float> vst3Buffer;
    juce::MidiBuffer vst3MidiBuffer;

    float gateEnv = 0.0f;
    float compEnv = 0.0f;
    float odPostFilterState = 0.0f;
    float ampStage1State = 0.0f;
    float ampStage2State = 0.0f;
    float preHpState = 0.0f;
    float delayDampL = 0.0f;
    float delayDampR = 0.0f;
};
