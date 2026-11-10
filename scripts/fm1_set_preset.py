#!/usr/bin/env python3
"""Send Program Change to FM-1 and print resulting parameters."""
import argparse
import glob
import sys
import time
import mido
import serial

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("program", type=int, default=5, nargs="?")
    a = ap.parse_args()
    names = mido.get_output_names()
    port = next((n for n in names if any(h.lower() in n.lower() for h in ["felucca", "fm-1", "noice"])), None)
    if not port:
        sys.exit(f"no FM-1 MIDI port in {names}")
    out = mido.open_output(port)
    out.send(mido.Message("program_change", channel=0, program=a.program))
    time.sleep(0.3)
    
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No /dev/cu.usbmodem* port found.")
    s = serial.Serial(ports[0], 115200, timeout=0.3)
    s.dtr = True
    time.sleep(0.1)
    s.reset_input_buffer()
    s.write(b"params\r\n")
    time.sleep(0.3)
    res = s.read(8192).decode("ascii", errors="replace")
    for line in res.splitlines():
        if "106=" in line:
            print(f"P_E0..P_E7 for program {a.program}: {line}")

if __name__ == "__main__":
    main()
