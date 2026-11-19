/**
 * Offline smoke: Demo → cloud reverb (+ Freeze segment) → WAV.
 * Usage: NeoCloudBurstSmoke [output.wav]
 */
#include "dsp/CloudReverbEngine.h"
#include "dsp/DemoToneGenerator.h"
#include <cmath>
#include <iostream>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outFile = (argc > 1)
        ? juce::File (argv[1])
        : juce::File::getSpecialLocation (juce::File::tempDirectory)
              .getChildFile ("neocloudburst-smoke.wav");

    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 256;
    constexpr int channels = 2;
    constexpr double totalSeconds = 8.0;
    constexpr double freezeAt = 3.5;
    constexpr double freezeOff = 6.5;

    DemoToneGenerator demo;
    CloudReverbEngine cloud;
    demo.prepare (sampleRate, channels);
    cloud.prepare (sampleRate, blockSize, channels);

    cloud.setDecay (0.78f);
    cloud.setPreDelayMs (48.0f);
    cloud.setTone (0.40f);
    cloud.setMod (0.35f);
    cloud.setFreeze (false);

    const int totalSamples = (int) std::round (totalSeconds * sampleRate);
    juce::AudioBuffer<float> master (channels, totalSamples);
    master.clear();

    juce::AudioBuffer<float> block (channels, blockSize);
    juce::AudioBuffer<float> dry (channels, blockSize);

    float mix = 0.55f;
    int written = 0;
    double peak = 0.0;
    double wetEnergy = 0.0;
    double dryEnergy = 0.0;
    int energySamples = 0;

    while (written < totalSamples)
    {
        const int n = juce::jmin (blockSize, totalSamples - written);
        block.setSize (channels, n, false, false, true);
        dry.setSize (channels, n, false, false, true);

        const double t0 = (double) written / sampleRate;
        cloud.setFreeze (t0 >= freezeAt && t0 < freezeOff);

        demo.render (block);
        for (int ch = 0; ch < channels; ++ch)
            dry.copyFrom (ch, 0, block, ch, 0, n);

        cloud.process (block);

        for (int i = 0; i < n; ++i)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                const float d = dry.getSample (ch, i);
                const float w = block.getSample (ch, i);
                const float out = d * (1.0f - mix) + w * mix;
                master.setSample (ch, written + i, out);
                peak = std::max (peak, (double) std::abs (out));
                dryEnergy += (double) (d * d);
                wetEnergy += (double) (w * w);
                ++energySamples;
            }
        }

        written += n;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        auto outStream = outFile.createOutputStream();
        if (outStream != nullptr)
        {
            writer.reset (wav.createWriterFor (outStream.get(), sampleRate, (unsigned int) channels, 24, {}, 0));
            if (writer != nullptr)
                outStream.release();
        }
    }

    if (writer == nullptr)
    {
        std::cerr << "Failed to open WAV for writing: " << outFile.getFullPathName() << std::endl;
        return 1;
    }

    if (! writer->writeFromAudioSampleBuffer (master, 0, totalSamples))
    {
        std::cerr << "Failed to write samples\n";
        return 2;
    }

    writer.reset();

    const double dryRms = std::sqrt (dryEnergy / juce::jmax (1, energySamples));
    const double wetRms = std::sqrt (wetEnergy / juce::jmax (1, energySamples));
    const double ratio = wetRms / juce::jmax (1.0e-9, dryRms);

    std::cout << "Wrote " << outFile.getFullPathName() << "\n"
              << "peak=" << peak
              << " dryRms=" << dryRms
              << " wetRms=" << wetRms
              << " wet/dry=" << ratio << std::endl;

    // Audible reverb should leave meaningful wet energy vs dry plucks
    if (peak < 0.01 || wetRms < 0.005 || ratio < 0.15)
    {
        std::cerr << "Smoke FAIL: reverb energy too low\n";
        return 3;
    }

    std::cout << "Smoke OK: ambient reverb + freeze segment rendered\n";
    return 0;
}
