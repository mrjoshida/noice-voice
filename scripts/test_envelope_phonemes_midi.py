#!/usr/bin/env python3
import time
import mido
from serial import Serial
from serial.tools import list_ports

print("Scanning for FM-1 devices...")
outputs = mido.get_output_names()
fm1_out = [o for o in outputs if "FM-1" in o or "Felucca" in o or "NOICE" in o]

if not fm1_out:
    print(f"Available MIDI outputs: {outputs}")
    exit(1)

port_name = fm1_out[0]
print(f"Connecting to MIDI: {port_name}")

with mido.open_output(port_name) as out:
    # Test GLIDE mode
    print("Testing Note ON / OFF with GLIDE mode...")
    out.send(mido.Message('note_on', note=57, velocity=100))
    time.sleep(0.6)
    out.send(mido.Message('note_off', note=57))
    time.sleep(0.2)

    # Test retroflex R [ɻ] (onset = 4)
    print("Testing Note with retroflex R [ɻ]...")
    out.send(mido.Message('note_on', note=60, velocity=110))
    time.sleep(0.7)
    out.send(mido.Message('note_off', note=60))
    time.sleep(0.3)

print("MIDI test notes completed successfully!")
