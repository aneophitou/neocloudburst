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

## Factory presets

Named presets live in `presets/factory-presets.json` and `Source/Presets/FactoryPresets.h` (keep in sync). Standalone UI preset menu is not wired yet — the offline showcase renderer loads the C++ table.

| Preset | Character |
|--------|-----------|
| Bright Room | Short bright ambience |
| Dark Cloud | Long dark wash |
| Slap Bloom | Audible pre-delay into the cloud |
| Swirl | High modulation |
| Freeze Pad | Freeze after a hit; dry continues |
| Infinite Wash | Near-infinite Decay, soft Mix |
| Soft Halo | Balanced everyday bloom |
| Wide Sky | Spacious sky pad |

### Presets showcase (phone)

Continuous Demo walkthrough with chime + spoken name markers:

- [`demos/neocloudburst-presets-showcase.m4a`](demos/neocloudburst-presets-showcase.m4a) (AAC)
- [`demos/neocloudburst-presets-showcase.wav`](demos/neocloudburst-presets-showcase.wav)

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
