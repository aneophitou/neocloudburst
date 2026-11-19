#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto kDemoOnId    = "demoOn";
constexpr auto kFreezeOnId  = "freezeOn";
constexpr auto kMixId       = "mix";
constexpr auto kDecayId     = "decay";
constexpr auto kPreDelayId  = "preDelay";
constexpr auto kToneId      = "tone";
constexpr auto kModId       = "mod";
} // namespace

NeoCloudBurstAudioProcessor::NeoCloudBurstAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout NeoCloudBurstAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kDemoOnId, 1 }, "Demo", false));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { kFreezeOnId, 1 }, "Freeze", false));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kMixId, 1 },
        "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.45f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.getFloatValue() / 100.0f;
            })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kDecayId, 1 },
        "Decay",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.72f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.getFloatValue() / 100.0f;
            })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kPreDelayId, 1 },
        "Pre-Delay",
        juce::NormalisableRange<float> { 0.0f, 250.0f, 0.1f, 0.45f },
        45.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kToneId, 1 },
        "Tone",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.42f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.getFloatValue() / 100.0f;
            })));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { kModId, 1 },
        "Mod",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.28f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) {
                return juce::String (juce::roundToInt (v * 100.0f));
            })
            .withValueFromStringFunction ([] (const juce::String& s) {
                return s.getFloatValue() / 100.0f;
            })));

    return { params.begin(), params.end() };
}

void NeoCloudBurstAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = getTotalNumOutputChannels();
    cloudEngine.prepare (sampleRate, samplesPerBlock, channels);
    dryBuffer.setSize (channels, samplesPerBlock, false, false, true);
    demoTone.prepare (sampleRate, channels);
    mixSmoothed.reset (sampleRate, 0.03);
    mixSmoothed.setCurrentAndTargetValue (0.45f);
    updateEnginesFromParameters();
}

void NeoCloudBurstAudioProcessor::releaseResources()
{
    cloudEngine.reset();
    demoTone.reset();
}

bool NeoCloudBurstAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOut = layouts.getMainOutputChannelSet();
    const auto& mainIn  = layouts.getMainInputChannelSet();

    if (mainOut != mainIn)
        return false;

    return mainOut == juce::AudioChannelSet::mono()
        || mainOut == juce::AudioChannelSet::stereo();
}

void NeoCloudBurstAudioProcessor::updateEnginesFromParameters()
{
    cloudEngine.setDecay (apvts.getRawParameterValue (kDecayId)->load());
    cloudEngine.setPreDelayMs (apvts.getRawParameterValue (kPreDelayId)->load());
    cloudEngine.setTone (apvts.getRawParameterValue (kToneId)->load());
    cloudEngine.setMod (apvts.getRawParameterValue (kModId)->load());
    cloudEngine.setFreeze (apvts.getRawParameterValue (kFreezeOnId)->load() > 0.5f);
    mixSmoothed.setTargetValue (apvts.getRawParameterValue (kMixId)->load());
}

void NeoCloudBurstAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    updateEnginesFromParameters();

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const bool demoOn = apvts.getRawParameterValue (kDemoOnId)->load() > 0.5f;
    if (demoOn)
        demoTone.render (buffer);

    dryBuffer.setSize (numChannels, numSamples, false, false, true);
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // Wet path: predelay → cloud tank → tone → mod (inside engine)
    cloudEngine.process (buffer);

    for (int i = 0; i < numSamples; ++i)
    {
        const float mix = mixSmoothed.getNextValue();
        const float dryGain = 1.0f - mix;
        const float wetGain = mix;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = dryBuffer.getSample (ch, i);
            const float wet = buffer.getSample (ch, i);
            buffer.setSample (ch, i, dry * dryGain + wet * wetGain);
        }
    }
}

juce::AudioProcessorEditor* NeoCloudBurstAudioProcessor::createEditor()
{
    return new NeoCloudBurstAudioProcessorEditor (*this);
}

void NeoCloudBurstAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NeoCloudBurstAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeoCloudBurstAudioProcessor();
}
