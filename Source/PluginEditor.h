#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>

class NeoCloudBurstAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit NeoCloudBurstAudioProcessorEditor (NeoCloudBurstAudioProcessor&);
    ~NeoCloudBurstAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label  title;
        juce::Label  value;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void layoutKnobs (juce::Rectangle<int> bounds, Knob* const* knobs, int count, int cols);
    void refreshToggleLook (juce::TextButton& button, bool isOn, juce::Colour onColour);
    void refreshDemoLook();
    void refreshFreezeLook();

    void setupKnob (Knob& knob,
                    const juce::String& name,
                    const juce::String& paramId,
                    std::function<juce::String (float)> formatter);

    void setupToggle (juce::TextButton& button,
                      const juce::String& label,
                      const juce::String& paramId,
                      std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment);

    NeoCloudBurstAudioProcessor& audioProcessor;

    juce::TextButton demoToggle { "Demo" };
    juce::TextButton freezeToggle { "Freeze" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> demoOnAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> freezeOnAttachment;

    Knob mixKnob;
    Knob decayKnob;
    Knob preDelayKnob;
    Knob toneKnob;
    Knob modKnob;

    float cloudPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeoCloudBurstAudioProcessorEditor)
};
