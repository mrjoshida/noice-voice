#!/usr/bin/env python3
"""Play the same phrase on several probe presets over USB MIDI, for an A/B listen on the FM-1.

  python3 scripts/fm1_ab.py --programs 5 0 4 3 [--repeat 2] [--gap 1.5]
Program numbers are TRACT_PRESETS indices. Part 1 must be on TRACT.
"""
import argparse
import sys
import time

import mido

PRESET_NAMES = {
    0: "AH (1X 22 sections @ 44.1kHz - clean)",
    1: "AH (2X 44 sections @ 88.2kHz - clean)",
    2: "EE (1X 22 sections @ 44.1kHz - clean)",
    3: "EE (2X 44 sections @ 88.2kHz - clean)",
    4: "CHOIR 1X (3-voice glottis 1X)",
    5: "CHOIR 2X (3-voice glottis 2X)",
    6: "NOISE TEST (turbulence + aspiration test)",
    7: "WORST 2X (worst case load)",
}

# (note, beats) — legato phrase with a held top note; 0 = rest
PHRASE = [(57, 1), (60, 1), (64, 1), (69, 2), (67, 1), (64, 1), (62, 1), (60, 3), (0, 1), (45, 2), (52, 2)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--programs", type=int, nargs="+", default=[0, 1, 2, 3],
                    help="Presets to play: 0=AH 1X, 1=AH 2X, 2=EE 1X, 3=EE 2X")
    ap.add_argument("--bpm", type=float, default=100.0)
    ap.add_argument("--gap", type=float, default=1.5)
    ap.add_argument("--repeat", type=int, default=1, help="Number of repetitions through all programs")
    ap.add_argument("--midi", nargs="+", default=["Felucca", "FM-1", "NOICE"])
    a = ap.parse_args()
    names = mido.get_output_names()
    port = next((n for n in names if any(h.lower() in n.lower() for h in a.midi)), None)
    if not port:
        sys.exit(f"no FM-1 MIDI port in {names}")
    out = mido.open_output(port)
    beat = 60.0 / a.bpm
    try:
        for r in range(a.repeat):
            if a.repeat > 1:
                print(f"\n--- Pass {r + 1}/{a.repeat} ---", flush=True)
            for pc in a.programs:
                desc = PRESET_NAMES.get(pc, f"Preset {pc}")
                print(f"Playing [{pc}]: {desc} ...", flush=True)
                out.send(mido.Message("program_change", channel=0, program=pc))
                time.sleep(0.3)
                prev = None
                for note, beats in PHRASE:
                    if note:
                        out.send(mido.Message("note_on", channel=0, note=note, velocity=100))
                    if prev:
                        out.send(mido.Message("note_off", channel=0, note=prev, velocity=0))   # legato: overlap
                    prev = note or None
                    time.sleep(beats * beat)
                if prev:
                    out.send(mido.Message("note_off", channel=0, note=prev, velocity=0))
                time.sleep(a.gap)
    finally:
        out.send(mido.Message("control_change", channel=0, control=123, value=0))
    print("Done.", flush=True)


if __name__ == "__main__":
    main()
