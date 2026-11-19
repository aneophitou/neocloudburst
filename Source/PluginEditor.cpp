#include "PluginEditor.h"

namespace
{
constexpr int kMinHit = 52;
constexpr int kPreferredWidth = 780;
constexpr int kPreferredHeight = 560;

const juce::Colour kBgTop      { 0xff1b2228 };
const juce::Colour kBgBottom   { 0xff0e1317 };
const juce::Colour kPanel      { 0xff243038 };
const juce::Colour kAccent     { 0xff7eb6c9 };
const juce::Colour kDemoAccent { 0xff8fbc8f };
const juce::Colour kFreezeAccent { 0xffc9a86a };
const juce::Colour kText       { 0xffe8eef2 };
const juce::Colour kMuted      { 0xff8a969e };
const juce::Colour kToggleOff  { 0xff1a2228 };

juce::String formatPercent (float v)
{
    return juce::String (juce::roundToInt (v * 100.0f)) + " %";
}
} // namespace

NeoCloudBurstAudioProcessorEditor::NeoCloudBurstAudioProcessorEditor (NeoCloudBurstAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (kPreferredWidth, kPreferredHeight);
    setResizable (true, true);
    setResizeLimits (640, 480, 1400, 1000);

    setupToggle (demoToggle,   "Demo",   "demoOn",   demoOnAttachment);
    setupToggle (freezeToggle, "Freeze", "freezeOn", freezeOnAttachment);
    refreshDemoLook();
    refreshFreezeLook();

    setupKnob (mixKnob, "Mix", "mix", formatPercent);
    setupKnob (decayKnob, "Decay", "decay", formatPercent);
    setupKnob (preDelayKnob, "Pre-Delay", "preDelay", [] (float v) {
        return juce::String (juce::roundToInt (v)) + " ms";
    });
    setupKnob (toneKnob, "Tone", "tone", formatPercent);
    setupKnob (modKnob, "Mod", "mod", formatPercent);

    startTimerHz (30);
}

void NeoCloudBurstAudioProcessorEditor::setupToggle (
    juce::TextButton& button,
    const juce::String& label,
    const juce::String& paramId,
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& attachment)
{
    button.setButtonText (label);
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::textColourOffId, kMuted);
    button.setColour (juce::TextButton::textColourOnId, kBgBottom);
    button.setColour (juce::TextButton::buttonColourId, kToggleOff);
    button.setColour (juce::TextButton::buttonOnColourId, kAccent);
    button.setConnectedEdges (0);
    addAndMakeVisible (button);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        audioProcessor.getAPVTS(), paramId, button);

    button.onClick = [this, &button]()
    {
        if (&button == &demoToggle)
            refreshDemoLook();
        else if (&button == &freezeToggle)
            refreshFreezeLook();
    };
}

void NeoCloudBurstAudioProcessorEditor::refreshToggleLook (juce::TextButton& button,
                                                            bool isOn,
                                                            juce::Colour onColour)
{
    button.setColour (juce::TextButton::buttonColourId, isOn ? onColour : kToggleOff);
    button.setColour (juce::TextButton::buttonOnColourId, onColour);
    button.setColour (juce::TextButton::textColourOffId, isOn ? kBgBottom : kMuted);
    button.setColour (juce::TextButton::textColourOnId, kBgBottom);
}

void NeoCloudBurstAudioProcessorEditor::refreshDemoLook()
{
    refreshToggleLook (demoToggle, demoToggle.getToggleState(), kDemoAccent);
}

void NeoCloudBurstAudioProcessorEditor::refreshFreezeLook()
{
    refreshToggleLook (freezeToggle, freezeToggle.getToggleState(), kFreezeAccent);
}

void NeoCloudBurstAudioProcessorEditor::setupKnob (Knob& knob,
                                                    const juce::String& name,
                                                    const juce::String& paramId,
                                                    std::function<juce::String (float)> formatter)
{
    knob.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    knob.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    knob.slider.setRotaryParameters (juce::degreesToRadians (225.0f),
                                     juce::degreesToRadians (495.0f),
                                     true);
    knob.slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
    knob.slider.setColour (juce::Slider::rotarySliderOutlineColourId, kMuted.withAlpha (0.55f));
    knob.slider.setColour (juce::Slider::thumbColourId, kText);
    addAndMakeVisible (knob.slider);

    knob.title.setText (name, juce::dontSendNotification);
    knob.title.setJustificationType (juce::Justification::centred);
    knob.title.setColour (juce::Label::textColourId, kText);
    knob.title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    addAndMakeVisible (knob.title);

    knob.value.setJustificationType (juce::Justification::centred);
    knob.value.setColour (juce::Label::textColourId, kMuted);
    knob.value.setFont (juce::FontOptions (13.0f));
    addAndMakeVisible (knob.value);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        audioProcessor.getAPVTS(), paramId, knob.slider);

    knob.slider.onValueChange = [&knob, formatter]()
    {
        knob.value.setText (formatter ((float) knob.slider.getValue()), juce::dontSendNotification);
    };

    knob.slider.onValueChange();
}

void NeoCloudBurstAudioProcessorEditor::timerCallback()
{
    const float mod = audioProcessor.getAPVTS().getRawParameterValue ("mod")->load();
    cloudPhase += (0.08f + mod * 0.35f) / 30.0f;
    if (cloudPhase >= 1.0f)
        cloudPhase -= 1.0f;

    refreshDemoLook();
    refreshFreezeLook();
    repaint (getLocalBounds().removeFromTop (64));
}

void NeoCloudBurstAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (kBgTop, 0.0f, 0.0f,
                                             kBgBottom, 0.0f, bounds.getHeight(), false));
    g.fillAll();

    // Soft atmospheric wash behind panel
    g.setColour (kAccent.withAlpha (0.06f + 0.04f * std::sin (cloudPhase * juce::MathConstants<float>::twoPi)));
    g.fillEllipse (bounds.getWidth() * 0.35f, -40.0f, bounds.getWidth() * 0.7f, 180.0f);

    auto panel = bounds.reduced (18.0f, 14.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (panel, 16.0f);
    g.setColour (kMuted.withAlpha (0.22f));
    g.drawRoundedRectangle (panel, 16.0f, 1.2f);

    g.setColour (kText);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("NeoCloudBurst", 28, 18, 320, 30, juce::Justification::centredLeft);

    g.setColour (kMuted);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText ("Andreas Neobyte  ·  ambient cloud reverb",
                28, 46, 360, 16, juce::Justification::centredLeft);

    const float indicatorX = (float) getWidth() - 150.0f;
    const float indicatorY = 38.0f;
    const float drift = std::sin (cloudPhase * juce::MathConstants<float>::twoPi) * 8.0f;
    g.setColour (kAccent.withAlpha (0.9f));
    g.fillEllipse (indicatorX + drift, indicatorY, 7.0f, 7.0f);
    g.setColour (kAccent.withAlpha (0.25f));
    g.fillEllipse (indicatorX + drift - 4.0f, indicatorY - 4.0f, 15.0f, 15.0f);
}

void NeoCloudBurstAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (28, 20);

    auto brandRow = area.removeFromTop (56);
    auto toggles = brandRow.removeFromRight (260);
    freezeToggle.setBounds (toggles.removeFromRight (120).withSizeKeepingCentre (118, juce::jmax (kMinHit - 6, 44)));
    toggles.removeFromRight (8);
    demoToggle.setBounds (toggles.removeFromRight (120).withSizeKeepingCentre (118, juce::jmax (kMinHit - 6, 44)));

    area.removeFromTop (12);

    Knob* knobs[] = { &mixKnob, &decayKnob, &preDelayKnob, &toneKnob, &modKnob };
    layoutKnobs (area, knobs, 5, 5);
}

void NeoCloudBurstAudioProcessorEditor::layoutKnobs (juce::Rectangle<int> bounds,
                                                     Knob* const* knobs,
                                                     int count,
                                                     int cols)
{
    const int rows = (count + cols - 1) / cols;
    const int cellW = bounds.getWidth() / cols;
    const int cellH = bounds.getHeight() / rows;

    for (int i = 0; i < count; ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        auto cell = juce::Rectangle<int> (bounds.getX() + col * cellW,
                                          bounds.getY() + row * cellH,
                                          cellW,
                                          cellH).reduced (8);

        auto titleArea = cell.removeFromTop (22);
        auto valueArea = cell.removeFromBottom (20);
        auto dialArea  = cell;

        const int dialSize = juce::jmax (kMinHit, juce::jmin (dialArea.getWidth(), dialArea.getHeight()) - 4);
        auto dial = juce::Rectangle<int> (dialSize, dialSize).withCentre (dialArea.getCentre());

        knobs[i]->title.setBounds (titleArea);
        knobs[i]->slider.setBounds (dial);
        knobs[i]->value.setBounds (valueArea);
    }
}
