#!/usr/bin/env python3
"""Test playing word sounds on NOICE VOICE over USB MIDI."""
import time
import mido

def main():
    outputs = mido.get_output_names()
    fm1_ports = [p for p in outputs if "Felucca" in p or "FM-1" in p or "NOICE" in p]
    if not fm1_ports:
        print("No FM-1 device found.")
        return
    port_name = fm1_ports[0]
    print(f"Connecting to {port_name}...")

    with mido.open_output(port_name) as out:
        words = [
            (1, "WEE (/u/ -> /i/)"),
            (2, "YOU (/i/ -> /u/)"),
            (3, "WHY (/a/ -> /i/)"),
            (4, "NOISE (/n/ + /o/ -> /i/)"),
            (5, "MA-MA (/m/ -> /a/)"),
        ]

        for prog, name in words:
            print(f"\nTesting Preset {prog}: {name}")
            out.send(mido.Message('program_change', program=prog))
            time.sleep(0.1)

            # Play a held note so the morph envelope articulates the word
            out.send(mido.Message('note_on', note=57, velocity=100)) # A3
            time.sleep(0.7)
            out.send(mido.Message('note_off', note=57))
            time.sleep(0.2)

        print("\nAll word tests complete!")

if __name__ == "__main__":
    main()
