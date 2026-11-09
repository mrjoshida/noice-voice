#!/usr/bin/env python3
"""Drive the FM-1 over USB to measure TRACT's CPU cost on the hardware (Phase 1b probe).

For every probe preset (MIDI Program Change on channel 1 -> part 1, which plays TRACT in the probe build), hold a
note over USB MIDI and read the serial console's `status` (cpu_pct, audio_max_us) a few times. Also reads the idle
load. Writes a JSON report and prints a table.

  python3 scripts/fm1_probe.py --out build/probe_report.json [--hold 4] [--note 57] [--chord]
"""
import argparse
import glob
import json
import re
import sys
import time

import mido
import serial

PRESETS = ["PROBE AH (2X solo)", "PROBE WORST (2X choir3)", "WORST 1X (choir3)", "SOLO 2X", "SOLO 1X", "AH 1X",
           "SOLO 2XN1", "WORST 2XN1 (choir3)"]


def find_midi_out(hint):
    names = mido.get_output_names()
    for n in names:
        if any(h.lower() in n.lower() for h in hint):
            return n
    sys.exit(f"no MIDI output matching {hint}: {names}")


def find_serial(path):
    if path:
        return path
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("no /dev/cu.usbmodem* serial console")
    return ports[0]


class Console:
    def __init__(self, path):
        self.s = serial.Serial(path, 115200, timeout=0.2)
        self.s.dtr = True
        time.sleep(0.2)
        self.s.reset_input_buffer()

    def cmd(self, line, wait=0.6):
        self.s.write((line + "\r\n").encode())
        t0, buf = time.time(), b""
        while time.time() - t0 < wait:
            buf += self.s.read(4096)
        return buf.decode(errors="replace")

    def status(self):
        txt = self.cmd("status")
        kv = dict(re.findall(r"^\s*([a-z0-9_]+)[ =:]+(-?\d+)\s*$", txt, re.M))
        return {k: int(v) for k, v in kv.items()}, txt


def measure(con, samples):
    vals = []
    for _ in range(samples):
        kv, _ = con.status()
        if "cpu_pct" in kv:
            vals.append((kv["cpu_pct"], kv.get("audio_max_us", -1)))
    return vals


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", default="build/probe_report.json")
    ap.add_argument("--serial", help="console device (default: first /dev/cu.usbmodem*)")
    ap.add_argument("--midi", nargs="+", default=["Felucca", "FM-1", "NOICE"], help="MIDI port name hints")
    ap.add_argument("--hold", type=float, default=4.0, help="seconds per preset")
    ap.add_argument("--note", type=int, default=57)
    ap.add_argument("--chord", action="store_true", help="hold three notes (legato part: only the last sounds)")
    ap.add_argument("--presets", type=int, nargs="*", help="program numbers to test (default: all)")
    ap.add_argument("--raw", action="store_true", help="print the first raw status reply")
    a = ap.parse_args()

    out = mido.open_output(find_midi_out(a.midi))
    con = Console(find_serial(a.serial))
    kv, txt = con.status()
    if a.raw or "cpu_pct" not in kv:
        print(txt)
    if "cpu_pct" not in kv:
        sys.exit("console did not answer `status` with cpu_pct")
    print(f"console: {txt.splitlines()[0] if txt.strip() else '?'}")
    report = {"idle": measure(con, 3), "presets": []}
    notes = [a.note, a.note + 4, a.note + 7] if a.chord else [a.note]
    for pc in a.presets if a.presets else range(len(PRESETS)):
        out.send(mido.Message("program_change", channel=0, program=pc))
        time.sleep(0.4)
        for n in notes:
            out.send(mido.Message("note_on", channel=0, note=n, velocity=100))
        time.sleep(1.0)
        vals = measure(con, max(2, int(a.hold / 0.7)))
        for n in notes:
            out.send(mido.Message("note_off", channel=0, note=n, velocity=0))
        time.sleep(0.6)
        name = PRESETS[pc] if pc < len(PRESETS) else f"program {pc}"
        cpu = [v[0] for v in vals]
        us = [v[1] for v in vals]
        row = {"program": pc, "name": name, "cpu_pct": cpu, "audio_max_us": us}
        report["presets"].append(row)
        print(f"{pc}  {name:26s} cpu {max(cpu) if cpu else '?':>3}%  (samples {cpu})  audio_max_us {max(us) if us else '?'}")
    out.send(mido.Message("control_change", channel=0, control=123, value=0))
    report["idle_after"] = measure(con, 2)
    print(f"idle: {report['idle']}  after: {report['idle_after']}")
    with open(a.out, "w") as f:
        json.dump(report, f, indent=1)
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
