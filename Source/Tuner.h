#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <atomic>

class GuitarTuner
{
public:
    GuitarTuner()
    {
        ringBuffer.assign (bufferSize, 0.0f);
        yinBuffer.assign (static_cast<size_t> (maxPeriod + 16), 0.0f);
    }

    void setSampleRate (double newSampleRate)
    {
        sampleRate = newSampleRate > 8000.0 ? newSampleRate : 48000.0;
        // Limit period range for guitar: ~60 Hz to ~1100 Hz
        minPeriod = static_cast<int> (sampleRate / 1100.0);
        maxPeriod = static_cast<int> (sampleRate / 60.0);
        if (maxPeriod >= bufferSize / 2)
            maxPeriod = bufferSize / 2 - 1;
        yinBuffer.assign (static_cast<size_t> (maxPeriod + 16), 0.0f);
    }

    void pushSamples (const float* samples, int numSamples)
    {
        if (samples == nullptr || numSamples <= 0)
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            ringBuffer[writePos] = samples[i];
            writePos = (writePos + 1) % bufferSize;
        }

        samplesSinceLastProcess += numSamples;
        // Process pitch roughly every 15-20 ms (~1024 samples at 48k)
        if (samplesSinceLastProcess >= 1024)
        {
            samplesSinceLastProcess = 0;
            processPitch();
        }
    }

    struct TunerResult
    {
        bool hasSignal = false;
        bool inTune = false;
        juce::String noteName = "--";
        int midiNote = 0;
        float cents = 0.0f;
        float frequency = 0.0f;
        float confidence = 0.0f;
    };

    TunerResult getLatestResult() const
    {
        TunerResult res;
        res.hasSignal = detectedHasSignal.load();
        res.inTune = detectedInTune.load();
        res.cents = detectedCents.load();
        res.frequency = detectedFreq.load();
        res.confidence = detectedConfidence.load();
        res.midiNote = detectedMidi.load();
        res.noteName = noteNameForMidi (res.midiNote);
        return res;
    }

    static juce::String noteNameForMidi (int midi)
    {
        if (midi <= 0 || midi > 127)
            return "--";
        static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        int noteIndex = (midi % 12 + 12) % 12;
        int octave = (midi / 12) - 1;
        return juce::String (noteNames[noteIndex]) + juce::String (octave);
    }

private:
    void processPitch()
    {
        // 1. Check RMS level to ensure there's guitar signal
        float sumSquares = 0.0f;
        int analysisLen = bufferSize / 2;
        int startPos = (writePos + bufferSize - analysisLen) % bufferSize;

        for (int i = 0; i < analysisLen; ++i)
        {
            float s = ringBuffer[(startPos + i) % bufferSize];
            sumSquares += s * s;
        }

        float rms = std::sqrt (sumSquares / static_cast<float> (analysisLen));
        if (rms < 0.0001f) // Ultra-sensitive: down to -80 dB
        {
            detectedHasSignal.store (false);
            detectedInTune.store (false);
            detectedCents.store (0.0f);
            detectedFreq.store (0.0f);
            detectedConfidence.store (0.0f);
            return;
        }

        // 2. YIN algorithm difference function
        yinBuffer[0] = 1.0f;
        float runningSum = 0.0f;

        for (int tau = 1; tau <= maxPeriod; ++tau)
        {
            float diff = 0.0f;
            for (int i = 0; i < analysisLen; ++i)
            {
                float x1 = ringBuffer[(startPos + i) % bufferSize];
                float x2 = ringBuffer[(startPos + i + tau) % bufferSize];
                float delta = x1 - x2;
                diff += delta * delta;
            }

            runningSum += diff;
            if (runningSum > 0.00001f)
                yinBuffer[tau] = diff * static_cast<float> (tau) / runningSum;
            else
                yinBuffer[tau] = 1.0f;
        }

        // 3. Absolute threshold search
        const float threshold = 0.15f;
        int bestTau = -1;

        for (int tau = minPeriod; tau <= maxPeriod; ++tau)
        {
            if (yinBuffer[tau] < threshold)
            {
                while (tau + 1 <= maxPeriod && yinBuffer[tau + 1] < yinBuffer[tau])
                    ++tau;
                bestTau = tau;
                break;
            }
        }

        // If no dip was below threshold, find global minimum in range
        if (bestTau == -1)
        {
            float minVal = 100.0f;
            for (int tau = minPeriod; tau <= maxPeriod; ++tau)
            {
                if (tau < static_cast<int> (yinBuffer.size()) && yinBuffer[tau] < minVal)
                {
                    minVal = yinBuffer[tau];
                    bestTau = tau;
                }
            }
            if (minVal > 0.35f) // Poor periodicity
            {
                detectedHasSignal.store (true);
                detectedConfidence.store (0.0f);
                return;
            }
        }

        if (bestTau <= 1 || bestTau >= maxPeriod || bestTau >= static_cast<int> (yinBuffer.size()) - 1)
            return;

        // 4. Parabolic interpolation around bestTau
        float refinedTau = static_cast<float> (bestTau);
        float s0 = yinBuffer[bestTau - 1];
        float s1 = yinBuffer[bestTau];
        float s2 = yinBuffer[bestTau + 1];
        float denominator = 2.0f * (2.0f * s1 - s0 - s2);
        if (std::abs (denominator) > 0.000001f)
        {
            float delta = (s2 - s0) / denominator;
            if (std::abs (delta) < 1.0f)
                refinedTau += delta;
        }

        float freq = static_cast<float> (sampleRate / refinedTau);
        if (freq < 55.0f || freq > 1200.0f)
            return;

        // MIDI note calculation
        float midi = 69.0f + 12.0f * std::log2 (freq / 440.0f);
        int nearestMidi = static_cast<int> (std::round (midi));
        float cents = (midi - static_cast<float> (nearestMidi)) * 100.0f;

        detectedHasSignal.store (true);
        detectedFreq.store (freq);
        detectedMidi.store (nearestMidi);
        detectedCents.store (cents);
        detectedInTune.store (std::abs (cents) <= 3.0f);
        detectedConfidence.store (1.0f - juce::jlimit (0.0f, 1.0f, yinBuffer[bestTau]));
    }

    static constexpr int bufferSize = 4096;
    double sampleRate = 48000.0;
    int minPeriod = 40;
    int maxPeriod = 800;

    std::vector<float> ringBuffer;
    int writePos = 0;
    int samplesSinceLastProcess = 0;
    std::vector<float> yinBuffer;

    std::atomic<bool> detectedHasSignal { false };
    std::atomic<bool> detectedInTune { false };
    std::atomic<int> detectedMidi { 0 };
    std::atomic<float> detectedCents { 0.0f };
    std::atomic<float> detectedFreq { 0.0f };
    std::atomic<float> detectedConfidence { 0.0f };
};

