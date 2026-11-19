#pragma once

#include <array>
#include <cstddef>

/**
 * Factory presets for NeoCloudBurst (Andreas Neobyte).
 * Mirrors presets/factory-presets.json — keep both in sync.
 * Standalone UI can load these later via APVTS; ShowcaseRender uses them now.
 */
struct FactoryPreset
{
    const char* id;
    const char* name;
    const char* description;
    float mix;
    float decay;
    float preDelayMs;
    float tone;
    float mod;
    bool  freeze;                 // initial freeze state
    float freezeAfterSeconds;     // if > 0, enable Freeze after this many seconds into the segment
    float showcaseSeconds;
};

inline constexpr std::array<FactoryPreset, 8> kFactoryPresets {{
    { "bright-room",    "Bright Room",    "Short bright ambience",
      0.28f, 0.28f,  18.0f, 0.82f, 0.12f, false, 0.0f, 5.5f },
    { "dark-cloud",     "Dark Cloud",     "Long dark cloud",
      0.62f, 0.88f,  55.0f, 0.18f, 0.22f, false, 0.0f, 7.0f },
    { "slap-bloom",     "Slap Bloom",     "Audible pre-delay slap into the cloud",
      0.48f, 0.68f, 165.0f, 0.55f, 0.18f, false, 0.0f, 6.5f },
    { "swirl",          "Swirl",          "Modulated swirl — high Mod",
      0.50f, 0.72f,  40.0f, 0.48f, 0.92f, false, 0.0f, 6.5f },
    { "freeze-pad",     "Freeze Pad",     "Freeze after a hit; dry continues",
      0.55f, 0.80f,  45.0f, 0.38f, 0.25f, false, 2.4f, 7.5f },
    { "infinite-wash",  "Infinite Wash",  "Near-infinite wash — max Decay, soft Mix",
      0.32f, 1.00f,  70.0f, 0.35f, 0.30f, false, 0.0f, 7.0f },
    { "soft-halo",      "Soft Halo",      "Balanced soft bloom behind the dry",
      0.40f, 0.58f,  35.0f, 0.45f, 0.28f, false, 0.0f, 5.5f },
    { "wide-sky",       "Wide Sky",       "Spacious sky pad",
      0.52f, 0.90f,  95.0f, 0.62f, 0.40f, false, 0.0f, 6.5f },
}};

inline constexpr std::size_t kNumFactoryPresets = kFactoryPresets.size();
