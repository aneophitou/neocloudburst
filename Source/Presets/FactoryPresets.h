#pragma once

#include <array>
#include <cstddef>

/**
 * Factory presets for NeoCloudBurst (Andreas Neobyte).
 * Mirrors presets/factory-presets.json — keep both in sync.
 * v2: airy / synth-like Freeze character; lighter Mix overall.
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
    bool  freeze;
    float freezeAfterSeconds;
    float showcaseSeconds;
};

inline constexpr std::array<FactoryPreset, 14> kFactoryPresets {{
    { "bright-room",    "Bright Room",    "Short bright ambience",
      0.26f, 0.26f,  16.0f, 0.84f, 0.10f, false, 0.0f, 5.0f },
    { "air-bloom",      "Air Bloom",      "Light airy bloom behind the dry",
      0.32f, 0.55f,  38.0f, 0.68f, 0.20f, false, 0.0f, 5.5f },
    { "soft-halo",      "Soft Halo",      "Balanced soft halo — everyday ambient",
      0.34f, 0.52f,  32.0f, 0.58f, 0.24f, false, 0.0f, 5.5f },
    { "slap-bloom",     "Slap Bloom",     "Audible pre-delay slap into a light cloud",
      0.38f, 0.62f, 155.0f, 0.62f, 0.16f, false, 0.0f, 6.0f },
    { "swirl",          "Swirl",          "Modulated swirl — high Mod, still airy",
      0.36f, 0.65f,  36.0f, 0.60f, 0.88f, false, 0.0f, 6.0f },
    { "glass-hold",     "Glass Hold",     "Subtle Freeze after a hit — glassy synth pad",
      0.30f, 0.72f,  42.0f, 0.72f, 0.18f, false, 2.2f, 7.5f },
    { "freeze-pad",     "Freeze Pad",     "Classic Freeze latch — light presence behind dry",
      0.28f, 0.78f,  48.0f, 0.66f, 0.22f, false, 2.3f, 7.5f },
    { "synth-bloom",    "Synth Bloom",    "Freeze-forward synth wash, soft Mix",
      0.26f, 0.85f,  55.0f, 0.78f, 0.28f, false, 2.0f, 7.0f },
    { "velvet-pad",     "Velvet Pad",     "Soft Freeze pad with gentle Mod shimmer",
      0.30f, 0.80f,  50.0f, 0.58f, 0.45f, false, 2.5f, 7.0f },
    { "infinite-wash",  "Infinite Wash",  "Near-infinite wash — max Decay, soft Mix",
      0.28f, 1.00f,  65.0f, 0.55f, 0.26f, false, 0.0f, 6.5f },
    { "wide-sky",       "Wide Sky",       "Spacious sky pad — airy Tone, medium Pre-Delay",
      0.36f, 0.86f,  90.0f, 0.70f, 0.35f, false, 0.0f, 6.0f },
    { "halo-drift",     "Halo Drift",     "Slow Mod drift over a light long cloud",
      0.33f, 0.82f,  60.0f, 0.64f, 0.70f, false, 0.0f, 6.0f },
    { "light-sustain",  "Light Sustain",  "Subtle Freeze sustain for clean leads",
      0.24f, 0.74f,  40.0f, 0.74f, 0.15f, false, 1.8f, 7.0f },
    { "dusk-veil",      "Dusk Veil",      "Slightly darker veil — still clear, not muddy",
      0.35f, 0.70f,  48.0f, 0.38f, 0.20f, false, 0.0f, 5.5f },
}};

inline constexpr std::size_t kNumFactoryPresets = kFactoryPresets.size();
