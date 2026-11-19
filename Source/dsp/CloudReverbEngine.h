#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

/**
 * Ambient cloud reverb (original DSP).
 *
 * Multi-delay FDN tank with input diffusion, variable pre-delay,
 * decay → near-infinite wash, tone tilt, slow modulation, and Freeze
 * latch with soft energy limiting (warm pad, not ice-pick).
 */
class CloudReverbEngine
{
public:
    CloudReverbEngine() = default;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void setDecay (float decay01);
    void setPreDelayMs (float ms);
    void setTone (float tone01);   // 0 = dark, 1 = bright
    void setMod (float mod01);
    void setFreeze (bool freezeOn);

    /** Processes wet-only: buffer in → reverb out (replaces buffer). */
    void process (juce::AudioBuffer<float>& buffer);

private:
    static constexpr int kNumDiffusers = 4;
    static constexpr int kFdnSize = 8;
    static constexpr float kMaxPreDelayMs = 250.0f;
    static constexpr float kModRateHz = 0.18f;
    static constexpr float kModDepthMs = 3.2f;

    struct Diffuser
    {
        std::vector<float> buffer;
        int index = 0;
        int size = 1;
        float z = 0.0f;
    };

    struct FdnLine
    {
        std::vector<float> buffer;
        int index = 0;
        int size = 1;
        float filterState = 0.0f;
        float modPhase = 0.0f;
    };

    float processDiffuser (Diffuser& d, float input) noexcept;
    void  processFdn (float inL, float inR, float& outL, float& outR) noexcept;
    float softLimit (float x) noexcept;
    float readDelay (const FdnLine& line, float delaySamples) const noexcept;

    double sampleRate = 44100.0;
    int numChannels = 2;

    std::array<Diffuser, kNumDiffusers> diffL {};
    std::array<Diffuser, kNumDiffusers> diffR {};
    std::array<FdnLine, kFdnSize> fdn {};

    std::vector<float> preDelayL;
    std::vector<float> preDelayR;
    int preDelayCapacity = 0;
    int preDelayWrite = 0;
    float preDelaySamples = 1.0f;

    float decayGain = 0.85f;
    float dampCoeff = 0.0f;
    float brightCoeff = 0.0f;
    float toneLpStateL = 0.0f;
    float toneLpStateR = 0.0f;
    float toneHpStateL = 0.0f;
    float toneHpStateR = 0.0f;
    float inputHpCoeff = 0.0f;
    std::array<float, 2> inputHpState { 0.0f, 0.0f };

    float modDepth = 0.0f;
    float modPhase = 0.0f;
    float modInc = 0.0f;

    bool freeze = false;
    float freezeSmoothed = 0.0f;
    float energyEnv = 0.0f;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> decaySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> toneSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> modSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> preDelaySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> freezeSmoother;
};
