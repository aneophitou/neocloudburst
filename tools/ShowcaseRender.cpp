/**
 * Continuous presets showcase: Demo tone through each factory preset,
 * with numbered chime markers + silence between segments.
 *
 * Usage:
 *   NeoCloudBurstShowcase <output.wav> [cues.json]
 *
 * Prints a cue sheet to stdout and optionally writes cues.json.
 */
#include "Presets/FactoryPresets.h"
#include "dsp/CloudReverbEngine.h"
#include "dsp/DemoToneGenerator.h"
#include <cmath>
#include <iostream>
#include <vector>

namespace
{
constexpr double kSampleRate = 44100.0;
constexpr int kBlockSize = 256;
constexpr int kChannels = 2;
constexpr float kMarkerSilenceSec = 0.75f;
constexpr float kGapDrySec = 0.45f;
constexpr float kBeepDurationSec = 0.09f;
constexpr float kBeepGapSec = 0.07f;

struct Cue
{
    juce::String name;
    juce::String id;
    double markerStartSec = 0.0;
    double audioStartSec = 0.0;
    double audioEndSec = 0.0;
};

void renderMarkerBeeps (juce::AudioBuffer<float>& dest, int startSample, int presetIndex)
{
    const int beepSamples = juce::jmax (1, (int) std::round (kBeepDurationSec * kSampleRate));
    const int gapSamples = juce::jmax (1, (int) std::round (kBeepGapSec * kSampleRate));
    const int count = presetIndex + 1;
    const float baseHz = 660.0f + 40.0f * (float) presetIndex;

    int cursor = startSample;
    for (int b = 0; b < count; ++b)
    {
        const float hz = baseHz + 55.0f * (float) b;
        for (int i = 0; i < beepSamples; ++i)
        {
            if (cursor + i >= dest.getNumSamples())
                return;

            const float t = (float) i / (float) kSampleRate;
            const float env = std::sin (juce::MathConstants<float>::pi * (float) i / (float) beepSamples);
            const float sample = 0.22f * env * std::sin (juce::MathConstants<float>::twoPi * hz * t);
            for (int ch = 0; ch < dest.getNumChannels(); ++ch)
                dest.addSample (ch, cursor + i, sample);
        }
        cursor += beepSamples + gapSamples;
    }
}

int markerLengthSamples (int presetIndex)
{
    const int beepSamples = juce::jmax (1, (int) std::round (kBeepDurationSec * kSampleRate));
    const int gapSamples = juce::jmax (1, (int) std::round (kBeepGapSec * kSampleRate));
    const int count = presetIndex + 1;
    const int beepsLen = count * beepSamples + (count - 1) * gapSamples;
    const int silence = juce::jmax (1, (int) std::round (kMarkerSilenceSec * kSampleRate));
    return beepsLen + silence;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outFile = (argc > 1)
        ? juce::File (argv[1])
        : juce::File::getSpecialLocation (juce::File::tempDirectory)
              .getChildFile ("neocloudburst-presets-showcase.wav");

    const juce::File cuesFile = (argc > 2)
        ? juce::File (argv[2])
        : outFile.getSiblingFile (outFile.getFileNameWithoutExtension() + "-cues.json");

    DemoToneGenerator demo;
    CloudReverbEngine cloud;
    demo.prepare (kSampleRate, kChannels);
    cloud.prepare (kSampleRate, kBlockSize, kChannels);

    // First pass: compute total length
    int totalSamples = 0;
    for (std::size_t i = 0; i < kNumFactoryPresets; ++i)
    {
        totalSamples += markerLengthSamples ((int) i);
        totalSamples += juce::jmax (1, (int) std::round (kGapDrySec * kSampleRate));
        totalSamples += juce::jmax (1, (int) std::round (kFactoryPresets[i].showcaseSeconds * kSampleRate));
    }
    // Trailing fade silence
    totalSamples += juce::jmax (1, (int) std::round (0.6 * kSampleRate));

    juce::AudioBuffer<float> master (kChannels, totalSamples);
    master.clear();

    juce::AudioBuffer<float> block (kChannels, kBlockSize);
    juce::AudioBuffer<float> dry (kChannels, kBlockSize);

    std::vector<Cue> cues;
    int writePos = 0;
    double peak = 0.0;

    for (std::size_t pi = 0; pi < kNumFactoryPresets; ++pi)
    {
        const auto& preset = kFactoryPresets[pi];
        Cue cue;
        cue.name = preset.name;
        cue.id = preset.id;
        cue.markerStartSec = (double) writePos / kSampleRate;

        // Marker beeps + silence
        const int markerLen = markerLengthSamples ((int) pi);
        renderMarkerBeeps (master, writePos, (int) pi);
        writePos += markerLen;

        // Short dry gap (silence already in marker tail + extra)
        const int gapSamples = juce::jmax (1, (int) std::round (kGapDrySec * kSampleRate));
        writePos += gapSamples;

        cue.audioStartSec = (double) writePos / kSampleRate;

        // Fresh tank so characters don't bleed
        cloud.reset();
        demo.reset();
        cloud.setDecay (preset.decay);
        cloud.setPreDelayMs (preset.preDelayMs);
        cloud.setTone (preset.tone);
        cloud.setMod (preset.mod);
        cloud.setFreeze (preset.freeze);

        const int segSamples = juce::jmax (1, (int) std::round (preset.showcaseSeconds * kSampleRate));
        const int freezeAtSample = (preset.freezeAfterSeconds > 0.0f)
            ? juce::jmax (1, (int) std::round (preset.freezeAfterSeconds * kSampleRate))
            : -1;

        int rendered = 0;
        while (rendered < segSamples)
        {
            const int n = juce::jmin (kBlockSize, segSamples - rendered);
            block.setSize (kChannels, n, false, false, true);
            dry.setSize (kChannels, n, false, false, true);

            if (freezeAtSample >= 0 && rendered >= freezeAtSample)
                cloud.setFreeze (true);

            demo.render (block);
            for (int ch = 0; ch < kChannels; ++ch)
                dry.copyFrom (ch, 0, block, ch, 0, n);

            cloud.process (block);

            const float mix = preset.mix;
            for (int i = 0; i < n; ++i)
            {
                // Soft edge fades at segment boundaries
                float edge = 1.0f;
                const int absPos = rendered + i;
                const int fadeIn = juce::jmin (2048, segSamples / 8);
                const int fadeOut = juce::jmin (4096, segSamples / 6);
                if (absPos < fadeIn)
                    edge = (float) absPos / (float) fadeIn;
                if (absPos > segSamples - fadeOut)
                    edge *= (float) (segSamples - absPos) / (float) fadeOut;

                for (int ch = 0; ch < kChannels; ++ch)
                {
                    const float d = dry.getSample (ch, i);
                    const float w = block.getSample (ch, i);
                    const float out = (d * (1.0f - mix) + w * mix) * edge;
                    master.addSample (ch, writePos + rendered + i, out);
                    peak = std::max (peak, (double) std::abs (out));
                }
            }

            rendered += n;
        }

        writePos += segSamples;
        cue.audioEndSec = (double) writePos / kSampleRate;
        cues.push_back (cue);

        std::cout << "CUE " << (pi + 1) << " " << preset.name
                  << " marker=" << juce::String (cue.markerStartSec, 2)
                  << "s audio=" << juce::String (cue.audioStartSec, 2)
                  << "-" << juce::String (cue.audioEndSec, 2) << "s\n";
    }

    // Gentle peak normalize if needed
    if (peak > 0.95)
    {
        const float gain = 0.92f / (float) peak;
        master.applyGain (gain);
        peak *= (double) gain;
    }

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    {
        auto outStream = outFile.createOutputStream();
        if (outStream != nullptr)
        {
            writer.reset (wav.createWriterFor (outStream.get(), kSampleRate, (unsigned int) kChannels, 24, {}, 0));
            if (writer != nullptr)
                outStream.release();
        }
    }

    if (writer == nullptr)
    {
        std::cerr << "Failed to open WAV: " << outFile.getFullPathName() << std::endl;
        return 1;
    }

    if (! writer->writeFromAudioSampleBuffer (master, 0, totalSamples))
    {
        std::cerr << "Failed to write samples\n";
        return 2;
    }
    writer.reset();

    // Cue sheet JSON
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty ("plugin", "NeoCloudBurst");
    root->setProperty ("author", "Andreas Neobyte");
    root->setProperty ("sampleRate", kSampleRate);
    root->setProperty ("durationSec", (double) totalSamples / kSampleRate);
    root->setProperty ("peak", peak);
    root->setProperty ("wav", outFile.getFileName());

    juce::Array<juce::var> cueArr;
    for (std::size_t i = 0; i < cues.size(); ++i)
    {
        juce::DynamicObject::Ptr o = new juce::DynamicObject();
        o->setProperty ("index", (int) i + 1);
        o->setProperty ("id", cues[i].id);
        o->setProperty ("name", cues[i].name);
        o->setProperty ("markerStartSec", cues[i].markerStartSec);
        o->setProperty ("audioStartSec", cues[i].audioStartSec);
        o->setProperty ("audioEndSec", cues[i].audioEndSec);
        cueArr.add (juce::var (o.get()));
    }
    root->setProperty ("cues", cueArr);

    const juce::String json = juce::JSON::toString (juce::var (root.get()), true);
    if (! cuesFile.replaceWithText (json))
    {
        std::cerr << "Warning: could not write cues JSON\n";
    }

    std::cout << "Wrote " << outFile.getFullPathName() << "\n"
              << "Cues " << cuesFile.getFullPathName() << "\n"
              << "duration=" << ((double) totalSamples / kSampleRate)
              << "s peak=" << peak
              << " presets=" << kNumFactoryPresets << std::endl;

    if (peak < 0.05)
    {
        std::cerr << "Showcase FAIL: peak too low\n";
        return 3;
    }

    return 0;
}
