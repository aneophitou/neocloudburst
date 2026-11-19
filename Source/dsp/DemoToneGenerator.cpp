#include "DemoToneGenerator.h"
#include <cmath>

namespace
{
constexpr float kFreqs[] = {
    146.83f, 174.61f, 196.00f, 220.00f,
    293.66f, 329.63f, 392.00f, 440.00f
};
constexpr int kPattern[] = { 0, 2, 4, 3, 4, 2, 5, 1, 0, 4, 6, 7, 5, 4, 2, 0 };
constexpr float kBpm = 72.0f;
} // namespace

void DemoToneGenerator::prepare (double newSampleRate, int newNumChannels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    numChannels = juce::jlimit (1, 2, newNumChannels);
    reset();
}

void DemoToneGenerator::reset()
{
    delayLine.fill (0.0f);
    period = 100;
    writePos = 0;
    prev = 0.0f;
    body = 0.0f;
    env = 0.0f;
    envInc = 0.0f;
    samplesUntilNext = 0;
    noteIndex = 0;
    pan = 0.0f;
    triggerNote (0);
}

void DemoToneGenerator::triggerNote (int patternIndex)
{
    const float freq = kFreqs[kPattern[patternIndex % (int) (sizeof (kPattern) / sizeof (kPattern[0]))]];
    period = juce::jlimit (2, kMaxPeriod - 1, (int) std::round (sampleRate / (double) freq));

    for (int i = 0; i < period; ++i)
        delayLine[(size_t) i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.85f;

    writePos = 0;
    prev = 0.0f;
    body = 0.0f;
    env = 1.0f;
    envInc = (float) (1.0 / (1.6 * sampleRate));
    pan = ((patternIndex & 1) != 0) ? 0.18f : -0.18f;

    const float beatSamples = (float) sampleRate * (60.0f / kBpm) * 0.5f;
    samplesUntilNext = juce::jmax (1, (int) std::round (beatSamples));
}

void DemoToneGenerator::render (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int channels = juce::jmin (buffer.getNumChannels(), numChannels);

    for (int i = 0; i < numSamples; ++i)
    {
        if (samplesUntilNext-- <= 0)
        {
            ++noteIndex;
            triggerNote (noteIndex);
        }

        const float filtered = 0.5f * (delayLine[(size_t) writePos] + prev);
        prev = delayLine[(size_t) writePos];
        delayLine[(size_t) writePos] = filtered * 0.9955f;
        writePos = (writePos + 1) % period;

        env = juce::jmax (0.0f, env - envInc);
        float sample = filtered * env * 0.42f;
        body = body * 0.96f + sample * 0.04f;
        sample = sample * 0.82f + body * 0.38f;

        const float l = sample * (0.72f - pan);
        const float r = sample * (0.72f + pan);

        buffer.setSample (0, i, l);
        if (channels > 1)
            buffer.setSample (1, i, r);
    }
}
