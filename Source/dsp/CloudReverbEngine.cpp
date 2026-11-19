#include "CloudReverbEngine.h"
#include <cmath>

namespace
{
int msToSamples (double sampleRate, float ms) noexcept
{
    return juce::jmax (2, (int) std::round (sampleRate * (double) ms * 0.001));
}

// Prime-ish diffuser / FDN lengths (ms) for dense, non-metallic bloom
constexpr float kDiffMs[4] = { 8.3f, 12.7f, 19.1f, 27.3f };
constexpr float kFdnMs[8]  = { 31.1f, 41.3f, 53.9f, 67.7f, 83.1f, 97.3f, 113.9f, 131.7f };

float onePoleCoeff (float hz, double sampleRate) noexcept
{
    const float safeHz = juce::jmax (20.0f, hz);
    return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * safeHz / (float) sampleRate);
}
} // namespace

void CloudReverbEngine::prepare (double newSampleRate, int /*maximumBlockSize*/, int newNumChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = juce::jlimit (1, 2, newNumChannels);

    for (int i = 0; i < kNumDiffusers; ++i)
    {
        auto setup = [&] (Diffuser& d, float ms)
        {
            d.size = msToSamples (sampleRate, ms);
            d.buffer.assign ((size_t) d.size, 0.0f);
            d.index = 0;
            d.z = 0.0f;
        };
        setup (diffL[(size_t) i], kDiffMs[i]);
        setup (diffR[(size_t) i], kDiffMs[i] * 1.09f);
    }

    for (int i = 0; i < kFdnSize; ++i)
    {
        auto& line = fdn[(size_t) i];
        const int base = msToSamples (sampleRate, kFdnMs[i]);
        line.size = base + msToSamples (sampleRate, kModDepthMs * 2.0f) + 8;
        line.buffer.assign ((size_t) line.size, 0.0f);
        line.index = 0;
        line.filterState = 0.0f;
        line.hpState = 0.0f;
        line.modPhase = (float) i / (float) kFdnSize;
    }

    preDelayCapacity = msToSamples (sampleRate, kMaxPreDelayMs) + 8;
    preDelayL.assign ((size_t) preDelayCapacity, 0.0f);
    preDelayR.assign ((size_t) preDelayCapacity, 0.0f);
    preDelayWrite = 0;
    preDelaySamples = msToSamples (sampleRate, 40.0f);

    // Higher input HPF — cloud stays behind the dry, less low-mid mud
    inputHpCoeff = onePoleCoeff (260.0f, sampleRate);
    // Freeze recirculation / output: scoop body, keep air
    freezeHpCoeff = onePoleCoeff (380.0f, sampleRate);
    freezeAirCoeff = onePoleCoeff (4200.0f, sampleRate);
    dampCoeff = onePoleCoeff (6800.0f, sampleRate);
    brightCoeff = onePoleCoeff (2200.0f, sampleRate);

    modInc = (float) (juce::MathConstants<double>::twoPi * (double) kModRateHz / sampleRate);

    decaySmoothed.reset (sampleRate, 0.08);
    toneSmoothed.reset (sampleRate, 0.05);
    modSmoothed.reset (sampleRate, 0.05);
    preDelaySmoothed.reset (sampleRate, 0.08);
    freezeSmoother.reset (sampleRate, 0.15);

    decaySmoothed.setCurrentAndTargetValue (0.62f);
    toneSmoothed.setCurrentAndTargetValue (0.58f);
    modSmoothed.setCurrentAndTargetValue (0.22f);
    preDelaySmoothed.setCurrentAndTargetValue (40.0f);
    freezeSmoother.setCurrentAndTargetValue (0.0f);

    reset();
}

void CloudReverbEngine::reset()
{
    for (auto& d : diffL)
    {
        std::fill (d.buffer.begin(), d.buffer.end(), 0.0f);
        d.index = 0;
        d.z = 0.0f;
    }
    for (auto& d : diffR)
    {
        std::fill (d.buffer.begin(), d.buffer.end(), 0.0f);
        d.index = 0;
        d.z = 0.0f;
    }
    for (auto& line : fdn)
    {
        std::fill (line.buffer.begin(), line.buffer.end(), 0.0f);
        line.index = 0;
        line.filterState = 0.0f;
        line.hpState = 0.0f;
    }

    std::fill (preDelayL.begin(), preDelayL.end(), 0.0f);
    std::fill (preDelayR.begin(), preDelayR.end(), 0.0f);
    preDelayWrite = 0;

    inputHpState = { 0.0f, 0.0f };
    freezeOutHpState = { 0.0f, 0.0f };
    freezeAirState = { 0.0f, 0.0f };
    toneLpStateL = toneLpStateR = 0.0f;
    toneHpStateL = toneHpStateR = 0.0f;
    modPhase = 0.0f;
    energyEnv = 0.0f;
    freezeSmoothed = 0.0f;
}

void CloudReverbEngine::setDecay (float decay01)
{
    decaySmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, decay01));
}

void CloudReverbEngine::setPreDelayMs (float ms)
{
    preDelaySmoothed.setTargetValue (juce::jlimit (0.0f, kMaxPreDelayMs, ms));
}

void CloudReverbEngine::setTone (float tone01)
{
    toneSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, tone01));
}

void CloudReverbEngine::setMod (float mod01)
{
    modSmoothed.setTargetValue (juce::jlimit (0.0f, 1.0f, mod01));
}

void CloudReverbEngine::setFreeze (bool freezeOn)
{
    freeze = freezeOn;
    freezeSmoother.setTargetValue (freezeOn ? 1.0f : 0.0f);
}

float CloudReverbEngine::softLimit (float x) noexcept
{
    // Gentler soft-limit — Freeze should bloom, not squash into a wall
    const float drive = 1.08f;
    const float y = std::tanh (x * drive);
    return y / std::tanh (drive);
}

float CloudReverbEngine::processDiffuser (Diffuser& d, float input) noexcept
{
    constexpr float g = 0.58f;
    const float bufOut = d.buffer[(size_t) d.index];
    const float y = -g * input + bufOut;
    d.buffer[(size_t) d.index] = input + g * bufOut;
    d.index = (d.index + 1) % d.size;
    d.z += 0.22f * (y - d.z);
    return d.z;
}

float CloudReverbEngine::readDelay (const FdnLine& line, float delaySamples) const noexcept
{
    const float maxDelay = (float) (line.size - 2);
    const float d = juce::jlimit (1.0f, maxDelay, delaySamples);
    float readPos = (float) line.index - d;
    while (readPos < 0.0f)
        readPos += (float) line.size;

    const int i0 = (int) readPos;
    const float frac = readPos - (float) i0;
    const int i1 = (i0 + 1) % line.size;
    return line.buffer[(size_t) i0] * (1.0f - frac) + line.buffer[(size_t) i1] * frac;
}

void CloudReverbEngine::processFdn (float inL, float inR, float& outL, float& outR) noexcept
{
    const float freezeAmt = freezeSmoothed;
    const float inputGain = 1.0f - freezeAmt;
    const float decay01 = decaySmoothed.getCurrentValue();

    // Map decay 0→1 to feedback; top end approaches near-infinite wash
    float feedback = 0.52f + decay01 * 0.44f;
    // Freeze latches high recirculation, but not quite brick-wall unity
    feedback = feedback + freezeAmt * (0.988f - feedback);
    feedback = juce::jlimit (0.0f, 0.992f, feedback);

    const float mod01 = modSmoothed.getCurrentValue();
    const float modSamples = mod01 * kModDepthMs * 0.001f * (float) sampleRate;

    std::array<float, kFdnSize> taps {};
    for (int i = 0; i < kFdnSize; ++i)
    {
        auto& line = fdn[(size_t) i];
        const float baseDelay = (float) msToSamples (sampleRate, kFdnMs[i]);
        const float lfo = std::sin (modPhase + line.modPhase * juce::MathConstants<float>::twoPi);
        taps[(size_t) i] = readDelay (line, baseDelay + lfo * modSamples);
    }

    float sum = 0.0f;
    for (float t : taps)
        sum += t;
    const float scale = 2.0f / (float) kFdnSize;

    std::array<float, kFdnSize> mixed {};
    for (int i = 0; i < kFdnSize; ++i)
        mixed[(size_t) i] = taps[(size_t) i] - scale * sum;

    // Slightly lighter inject — bloom sits behind dry
    const float injectL = inL * inputGain * 0.13f;
    const float injectR = inR * inputGain * 0.13f;

    // Tighter energy governor when frozen so the pad stays subtle
    float tankEnergy = 0.0f;
    for (float t : taps)
        tankEnergy += t * t;
    tankEnergy = std::sqrt (tankEnergy / (float) kFdnSize);
    energyEnv = energyEnv * 0.992f + tankEnergy * 0.008f;
    const float energyLimit = 0.38f - freezeAmt * 0.12f; // ~0.26 when frozen
    float energyGain = 1.0f;
    if (energyEnv > energyLimit)
        energyGain = energyLimit / juce::jmax (energyEnv, 1.0e-4f);

    // Freeze BRIGHTENS the loop (was darkening) — glassy synth pad, not mud
    const float dampHz = 7000.0f + freezeAmt * 2200.0f - (1.0f - decay01) * 600.0f;
    const float localDamp = onePoleCoeff (dampHz, sampleRate);
    const float loopHpCoeff = onePoleCoeff (280.0f + freezeAmt * 220.0f, sampleRate);

    for (int i = 0; i < kFdnSize; ++i)
    {
        auto& line = fdn[(size_t) i];
        const float input = (i & 1) ? injectR : injectL;
        float sample = mixed[(size_t) i] * feedback * energyGain + input;

        // Low-mid scoop in the recirculation (stronger when Freeze is on)
        line.hpState += loopHpCoeff * (sample - line.hpState);
        const float scooped = sample - line.hpState * (0.35f + freezeAmt * 0.55f);
        sample = scooped;

        line.filterState += localDamp * (sample - line.filterState);
        sample = softLimit (line.filterState * (1.0f - freezeAmt * 0.12f));

        line.buffer[(size_t) line.index] = sample;
        line.index = (line.index + 1) % line.size;
    }

    outL = 0.0f;
    outR = 0.0f;
    for (int i = 0; i < kFdnSize; i += 2)
    {
        outL += taps[(size_t) i];
        outR += taps[(size_t) i + 1];
    }
    // Quieter wet pickup; Freeze attenuates further so dry stays clear
    const float outGain = 0.22f * (1.0f - freezeAmt * 0.35f);
    outL *= outGain;
    outR *= outGain;
}

void CloudReverbEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        freezeSmoothed = freezeSmoother.getNextValue();
        const float decay01 = decaySmoothed.getNextValue();
        const float tone01 = toneSmoothed.getNextValue();
        const float mod01 = modSmoothed.getNextValue();
        const float preMs = preDelaySmoothed.getNextValue();
        juce::ignoreUnused (decay01, mod01);

        preDelaySamples = juce::jlimit (1.0f, (float) (preDelayCapacity - 2),
                                        preMs * 0.001f * (float) sampleRate);

        float inL = buffer.getSample (0, i);
        float inR = channels > 1 ? buffer.getSample (1, i) : inL;

        inputHpState[0] += inputHpCoeff * (inL - inputHpState[0]);
        inputHpState[1] += inputHpCoeff * (inR - inputHpState[1]);
        float hpL = inL - inputHpState[0];
        float hpR = inR - inputHpState[1];

        float readPos = (float) preDelayWrite - preDelaySamples;
        while (readPos < 0.0f)
            readPos += (float) preDelayCapacity;
        const int i0 = (int) readPos;
        const float frac = readPos - (float) i0;
        const int i1 = (i0 + 1) % preDelayCapacity;
        const float pdL = preDelayL[(size_t) i0] * (1.0f - frac) + preDelayL[(size_t) i1] * frac;
        const float pdR = preDelayR[(size_t) i0] * (1.0f - frac) + preDelayR[(size_t) i1] * frac;
        preDelayL[(size_t) preDelayWrite] = hpL;
        preDelayR[(size_t) preDelayWrite] = hpR;
        preDelayWrite = (preDelayWrite + 1) % preDelayCapacity;

        float dL = pdL;
        float dR = pdR;
        for (int d = 0; d < kNumDiffusers; ++d)
        {
            dL = processDiffuser (diffL[(size_t) d], dL);
            dR = processDiffuser (diffR[(size_t) d], dR);
        }

        float revL = 0.0f, revR = 0.0f;
        processFdn (dL, dR, revL, revR);

        // Tone tilt — bias mid/bright; dark end is less muddy than v1
        const float dark = 1.0f - tone01;
        const float bright = tone01;
        toneLpStateL += brightCoeff * (revL - toneLpStateL);
        toneLpStateR += brightCoeff * (revR - toneLpStateR);
        float tonedL = toneLpStateL * (0.25f + dark * 0.55f) + revL * (0.45f + bright * 0.55f);
        float tonedR = toneLpStateR * (0.25f + dark * 0.55f) + revR * (0.45f + bright * 0.55f);

        // Air shelf (stronger overall, extra when Freeze is on)
        toneHpStateL += onePoleCoeff (3200.0f, sampleRate) * (tonedL - toneHpStateL);
        toneHpStateR += onePoleCoeff (3200.0f, sampleRate) * (tonedR - toneHpStateR);
        const float airL = tonedL - toneHpStateL;
        const float airR = tonedR - toneHpStateR;
        const float airAmt = 0.22f + bright * 0.32f + freezeSmoothed * 0.18f;
        tonedL += airL * airAmt;
        tonedR += airR * airAmt;

        // Freeze output: extra HPF + gentle presence — synth bloom, not drowned wall
        freezeOutHpState[0] += freezeHpCoeff * (tonedL - freezeOutHpState[0]);
        freezeOutHpState[1] += freezeHpCoeff * (tonedR - freezeOutHpState[1]);
        const float freezeHpL = tonedL - freezeOutHpState[0];
        const float freezeHpR = tonedR - freezeOutHpState[1];
        tonedL += (freezeHpL - tonedL) * freezeSmoothed * 0.55f;
        tonedR += (freezeHpR - tonedR) * freezeSmoothed * 0.55f;

        freezeAirState[0] += freezeAirCoeff * (tonedL - freezeAirState[0]);
        freezeAirState[1] += freezeAirCoeff * (tonedR - freezeAirState[1]);
        tonedL += (tonedL - freezeAirState[0]) * freezeSmoothed * 0.22f;
        tonedR += (tonedR - freezeAirState[1]) * freezeSmoothed * 0.22f;

        modPhase += modInc;
        if (modPhase >= juce::MathConstants<float>::twoPi)
            modPhase -= juce::MathConstants<float>::twoPi;

        buffer.setSample (0, i, softLimit (tonedL));
        if (channels > 1)
            buffer.setSample (1, i, softLimit (tonedR));
    }
}
