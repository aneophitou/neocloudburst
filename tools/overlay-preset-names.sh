#!/usr/bin/env bash
# Overlay espeak-ng spoken preset names onto showcase WAV at cue marker times.
# Usage: overlay-preset-names.sh <showcase.wav> <cues.json> <output.wav>
set -euo pipefail

IN_WAV="${1:?wav}"
CUES="${2:?cues.json}"
OUT_WAV="${3:?out wav}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

python3 - <<PY
import json, subprocess, os, wave, struct, math
from pathlib import Path

cues = json.loads(Path("$CUES").read_text())
work = Path("$WORK")
sr = 44100
inputs = ["$IN_WAV"]
filter_parts = []
# base = [0:a]
idx = 0
delayed = []
for i, c in enumerate(cues["cues"]):
    name = c["name"]
    start_ms = int(round(float(c["markerStartSec"]) * 1000))
    wav_path = work / f"name_{i:02d}.wav"
    # Soft, clear announcement
    subprocess.run([
        "espeak-ng", "-v", "en-us", "-s", "140", "-a", "70",
        "-w", str(wav_path), name
    ], check=True)
    # Ensure stereo 44.1k
    stereo = work / f"name_{i:02d}_st.wav"
    subprocess.run([
        "ffmpeg", "-y", "-i", str(wav_path),
        "-ar", "44100", "-ac", "2", "-c:a", "pcm_s16le", str(stereo)
    ], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    inputs.append(str(stereo))
    delayed.append((len(inputs)-1, start_ms))

# Build filter: adelay each spoken clip then amix
n = len(inputs)
fc = []
for j, (inp_i, ms) in enumerate(delayed):
    fc.append(f"[{inp_i}:a]adelay={ms}|{ms},volume=0.85[s{j}]")
mix_ins = "[0:a]" + "".join(f"[s{j}]" for j in range(len(delayed)))
fc.append(f"{mix_ins}amix=inputs={1+len(delayed)}:duration=first:dropout_transition=0:normalize=0[aout]")
filter_complex = ";".join(fc)

cmd = ["ffmpeg", "-y"]
for p in inputs:
    cmd += ["-i", p]
cmd += ["-filter_complex", filter_complex, "-map", "[aout]", "-c:a", "pcm_s24le", "$OUT_WAV"]
print(" ".join(cmd[:6]), "...")
subprocess.run(cmd, check=True)
print("Wrote", "$OUT_WAV")
PY
