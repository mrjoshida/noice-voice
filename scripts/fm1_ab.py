#!/usr/bin/env python3
"""Play the same phrase on several probe presets over USB MIDI, for an A/B listen on the FM-1.

  python3 scripts/fm1_ab.py --programs 0 5 6 [--gap 1.5]
Program numbers are TRACT_PRESETS indices (scripts/fm1_probe.py PRESETS). Part 1 must be on TRACT.
"""
import argparse
import sys
import time

import mido

# (note, beats) — legato phrase with a held top note; 0 = rest
PHRASE = [(57, 1), (60, 1), (64, 1), (69, 2), (67, 1), (64, 1), (62, 1), (60, 3), (0, 1), (45, 2), (52, 2)]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--programs", type=int, nargs="+", default=[0, 5, 6])
    ap.add_argument("--bpm", type=float, default=100.0)
    ap.add_argument("--gap", type=float, default=1.5)
    ap.add_argument("--midi", nargs="+", default=["Felucca", "FM-1", "NOICE"])
    a = ap.parse_args()
    names = mido.get_output_names()
    port = next((n for n in names if any(h.lower() in n.lower() for h in a.midi)), None)
    if not port:
        sys.exit(f"no FM-1 MIDI port in {names}")
    out = mido.open_output(port)
    beat = 60.0 / a.bpm
    for pc in a.programs:
        print(f"program {pc}", flush=True)
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
    out.send(mido.Message("control_change", channel=0, control=123, value=0))


if __name__ == "__main__":
    main()
