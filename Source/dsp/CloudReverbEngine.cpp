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
        // Extra headroom for modulation read-ahead
        const int base = msToSamples (sampleRate, kFdnMs[i]);
        line.size = base + msToSamples (sampleRate, kModDepthMs * 2.0f) + 8;
        line.buffer.assign ((size_t) line.size, 0.0f);
        line.index = 0;
        line.filterState = 0.0f;
        line.modPhase = (float) i / (float) kFdnSize;
    }

    preDelayCapacity = msToSamples (sampleRate, kMaxPreDelayMs) + 8;
    preDelayL.assign ((size_t) preDelayCapacity, 0.0f);
    preDelayR.assign ((size_t) preDelayCapacity, 0.0f);
    preDelayWrite = 0;
    preDelaySamples = msToSamples (sampleRate, 40.0f);

    inputHpCoeff = onePoleCoeff (120.0f, sampleRate);
    dampCoeff = onePoleCoeff (5200.0f, sampleRate);
    brightCoeff = onePoleCoeff (1800.0f, sampleRate);

    modInc = (float) (juce::MathConstants<double>::twoPi * (double) kModRateHz / sampleRate);

    decaySmoothed.reset (sampleRate, 0.08);
    toneSmoothed.reset (sampleRate, 0.05);
    modSmoothed.reset (sampleRate, 0.05);
    preDelaySmoothed.reset (sampleRate, 0.08);
    freezeSmoother.reset (sampleRate, 0.12);

    decaySmoothed.setCurrentAndTargetValue (0.65f);
    toneSmoothed.setCurrentAndTargetValue (0.45f);
    modSmoothed.setCurrentAndTargetValue (0.25f);
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
    }

    std::fill (preDelayL.begin(), preDelayL.end(), 0.0f);
    std::fill (preDelayR.begin(), preDelayR.end(), 0.0f);
    preDelayWrite = 0;

    inputHpState = { 0.0f, 0.0f };
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
    // Soft tanh-ish limiter — warm compression, no hard clip spikes
    const float drive = 1.35f;
    const float y = std::tanh (x * drive);
    return y / std::tanh (drive);
}

float CloudReverbEngine::processDiffuser (Diffuser& d, float input) noexcept
{
    constexpr float g = 0.62f;
    const float bufOut = d.buffer[(size_t) d.index];
    const float y = -g * input + bufOut;
    d.buffer[(size_t) d.index] = input + g * bufOut;
    d.index = (d.index + 1) % d.size;
    d.z += 0.28f * (y - d.z);
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

    // Map decay 0→1 to feedback; top end approaches ~0.995 (near-infinite wash)
    float feedback = 0.55f + decay01 * 0.42f;
    // Freeze latches near-unity recirculation
    feedback = feedback + freezeAmt * (0.995f - feedback);
    feedback = juce::jlimit (0.0f, 0.997f, feedback);

    // Modulation depth in samples
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

    // Householder-style dense mix
    float sum = 0.0f;
    for (float t : taps)
        sum += t;
    const float scale = 2.0f / (float) kFdnSize;

    std::array<float, kFdnSize> mixed {};
    for (int i = 0; i < kFdnSize; ++i)
        mixed[(size_t) i] = taps[(size_t) i] - scale * sum;

    const float injectL = inL * inputGain * 0.16f;
    const float injectR = inR * inputGain * 0.16f;

    // Soft energy governor — keep freeze/long decay warm, not runaway
    float tankEnergy = 0.0f;
    for (float t : taps)
        tankEnergy += t * t;
    tankEnergy = std::sqrt (tankEnergy / (float) kFdnSize);
    energyEnv = energyEnv * 0.995f + tankEnergy * 0.005f;
    const float energyLimit = 0.55f + freezeAmt * 0.15f;
    float energyGain = 1.0f;
    if (energyEnv > energyLimit)
        energyGain = energyLimit / juce::jmax (energyEnv, 1.0e-4f);

    // HF damping in the loop (warmer when dark / frozen)
    const float dampHz = 6200.0f - freezeAmt * 1800.0f - (1.0f - decay01) * 800.0f;
    const float localDamp = onePoleCoeff (dampHz, sampleRate);

    for (int i = 0; i < kFdnSize; ++i)
    {
        auto& line = fdn[(size_t) i];
        const float input = (i & 1) ? injectR : injectL;
        float sample = mixed[(size_t) i] * feedback * energyGain + input;

        line.filterState += localDamp * (sample - line.filterState);
        sample = softLimit (line.filterState);

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
    outL *= 0.28f;
    outR *= 0.28f;
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

        // Mild input HPF so the cloud sits behind the dry guitar
        inputHpState[0] += inputHpCoeff * (inL - inputHpState[0]);
        inputHpState[1] += inputHpCoeff * (inR - inputHpState[1]);
        float hpL = inL - inputHpState[0];
        float hpR = inR - inputHpState[1];

        // Variable pre-delay (linear interp)
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

        // Tone tilt on wet path: dark = more LP, bright = mild shelf / less damp
        const float dark = 1.0f - tone01;
        const float bright = tone01;
        toneLpStateL += brightCoeff * (revL - toneLpStateL);
        toneLpStateR += brightCoeff * (revR - toneLpStateR);
        float tonedL = toneLpStateL * (0.35f + dark * 0.65f) + revL * (0.35f + bright * 0.65f);
        float tonedR = toneLpStateR * (0.35f + dark * 0.65f) + revR * (0.35f + bright * 0.65f);

        // Subtle high-shelf for bright end (still soft, not icy)
        toneHpStateL += onePoleCoeff (3500.0f, sampleRate) * (tonedL - toneHpStateL);
        toneHpStateR += onePoleCoeff (3500.0f, sampleRate) * (tonedR - toneHpStateR);
        const float airL = tonedL - toneHpStateL;
        const float airR = tonedR - toneHpStateR;
        tonedL += airL * bright * 0.28f;
        tonedR += airR * bright * 0.28f;

        // Slow diffusion / pitch wobble presence via LFO advance
        modPhase += modInc;
        if (modPhase >= juce::MathConstants<float>::twoPi)
            modPhase -= juce::MathConstants<float>::twoPi;

        buffer.setSample (0, i, softLimit (tonedL));
        if (channels > 1)
            buffer.setSample (1, i, softLimit (tonedR));
    }
}
