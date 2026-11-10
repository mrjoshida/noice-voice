#!/usr/bin/env python3
"""Test the 4 creature engines over USB MIDI and CDC."""
import time
import mido
import serial
import glob
import sys

def get_status():
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        return "No port"
    try:
        s = serial.Serial(ports[0], 115200, timeout=0.3)
        s.dtr = True
        time.sleep(0.05)
        s.reset_input_buffer()
        s.write(b"status\r\n")
        time.sleep(0.15)
        res = s.read(4096).decode("ascii", errors="replace")
        s.close()
        lines = [line.strip() for line in res.splitlines() if line.startswith(("track", "engine", "preset", "cpu_pct"))]
        return ", ".join(lines)
    except Exception as e:
        return str(e)

def main():
    outputs = mido.get_output_names()
    fm1_ports = [p for p in outputs if any(h in p for h in ["FM-1", "Felucca", "NOICE"])]
    if not fm1_ports:
        sys.exit(f"No FM-1 MIDI port found in: {outputs}")

    port_name = fm1_ports[0]
    print(f"Connecting to MIDI: {port_name}")

    with mido.open_output(port_name) as out:
        creatures = [
            ("Track 1: HUMAN", 0, 60, "AH MALE (C4)"),
            ("Track 2: TROLL", 1, 48, "DEEP GROWL (C3)"),
            ("Track 3: ELF",   2, 72, "SILVER (C5)"),
            ("Track 4: BEAST", 3, 55, "SNARL (G3)"),
        ]

        for name, ch, note, desc in creatures:
            print(f"\n--- Testing {name} ---")
            print(f"Playing {desc} on MIDI channel {ch + 1}...")

            # Program change 0 (first preset)
            out.send(mido.Message('program_change', channel=ch, program=0))
            time.sleep(0.1)

            # Play a short phrase of 3 notes
            for pitch in [note, note + 2, note + 4]:
                out.send(mido.Message('note_on', channel=ch, note=pitch, velocity=100))
                time.sleep(0.35)
                out.send(mido.Message('note_off', channel=ch, note=pitch))
                time.sleep(0.05)

            time.sleep(0.2)
            print("Status:", get_status())

if __name__ == "__main__":
    main()
