# NeoCloudBurst

Soft ambient **cloud reverb** with Freeze for Linux — **Standalone**, **LV2**, and **VST3**, built with **JUCE 8**.

By **Andreas Neobyte**. Inspired by Strymon Cloudburst character; original DSP and UI.

## Controls (v1)

| Control | Role |
|---------|------|
| **Mix** | Dry / wet |
| **Decay** | Reverb length (top → near-infinite wash) |
| **Pre-Delay** | Time before the cloud |
| **Tone** | Dark ↔ bright tilt on the wet path |
| **Mod** | Slow pitch / diffusion modulation depth |
| **Freeze** | Latch: holds the cloud pad while dry continues |
| **Demo** | Built-in plucked tone (no guitar required) |

Signal: Input/Demo → dry + predelay → FDN cloud tank → tone → mod → mix. Freeze recirculates the tank with soft limiting.

## Factory presets (v2)

Named presets live in `presets/factory-presets.json` and `Source/Presets/FactoryPresets.h` (keep in sync). Freeze/cloud is tuned as a **light synth-like bloom** behind the dry. Standalone UI preset menu is not wired yet — the offline showcase renderer loads the C++ table.

| Preset | Character |
|--------|-----------|
| Bright Room | Short bright ambience |
| Air Bloom | Light airy bloom |
| Soft Halo | Everyday soft halo |
| Slap Bloom | Audible pre-delay into a light cloud |
| Swirl | High Mod, still airy |
| Glass Hold | Subtle Freeze — glassy synth pad |
| Freeze Pad | Classic Freeze, light behind dry |
| Synth Bloom | Freeze-forward synth wash |
| Velvet Pad | Soft Freeze + gentle Mod |
| Infinite Wash | Near-infinite Decay, soft Mix |
| Wide Sky | Spacious airy sky pad |
| Halo Drift | Slow Mod over light long cloud |
| Light Sustain | Subtle Freeze for clean leads |
| Dusk Veil | Slightly darker veil, still clear |

### Presets showcase (phone)

**v2** (softer Freeze) — continuous Demo walkthrough with chime + spoken name markers:

- [`demos/neocloudburst-presets-showcase-v2.m4a`](demos/neocloudburst-presets-showcase-v2.m4a) (AAC)
- [`demos/neocloudburst-presets-showcase-v2.wav`](demos/neocloudburst-presets-showcase-v2.wav)

Release: [`demo-presets-2`](https://github.com/aneophitou/neocloudburst/releases/tag/demo-presets-2)

```bash
./build/NeoCloudBurstShowcase_artefacts/Release/NeoCloudBurstShowcase /tmp/out.wav /tmp/cues.json
./tools/overlay-preset-names.sh /tmp/out.wav /tmp/cues.json /tmp/named.wav
```

## Requirements

- Linux (x86-64 or aarch64)
- CMake ≥ 3.22, C++17, Ninja recommended
- Typical packages:

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  libasound2-dev libfreetype-dev libx11-dev libxcomposite-dev \
  libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev \
  libxrender-dev libgl1-mesa-dev libcurl4-openssl-dev lv2-dev
```

JUCE is fetched via CMake `FetchContent`.

## Build

```bash
cd NeoCloudBurst
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
# On low-memory boards, build juceaide single-threaded first if configure OOMs:
#   ninja -C build/_deps/juce-build/tools -j1 juceaide
cmake --build build -j$(nproc)
```

Artifacts:

| Format | Path |
|--------|------|
| Standalone | `build/NeoCloudBurst_artefacts/Release/Standalone/NeoCloudBurst` |
| LV2 | `Dist/LV2/NeoCloudBurst.lv2` |
| VST3 | `Dist/VST3/NeoCloudBurst.vst3` |

### Smoke render (no audio device)

```bash
./build/NeoCloudBurstSmoke_artefacts/Release/NeoCloudBurstSmoke /tmp/neocloudburst-smoke.wav
```

### Legion (later)

When the Legion self-hosted worker is online:

```bash
cd ~/neocloudburst   # or wherever the tree lives
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/NeoCloudBurst_artefacts/Release/Standalone/NeoCloudBurst
```

Enable **Demo** in the UI to hear ambient reverb without a guitar. Toggle **Freeze** to latch the cloud.

Pi / PipeDal aarch64 LV2 comes after Standalone sound is approved — do not deploy early.

## License

All rights reserved unless otherwise noted.
