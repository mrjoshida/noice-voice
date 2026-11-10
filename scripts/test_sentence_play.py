#!/usr/bin/env python3
import time
import mido

print("Scanning for FM-1 USB MIDI...")
outputs = mido.get_output_names()
fm1_out = [o for o in outputs if "FM-1" in o or "Felucca" in o or "NOICE" in o]

if not fm1_out:
    print(f"Available MIDI outputs: {outputs}")
    print("Could not find FM-1 MIDI output device.")
    exit(1)

port_name = fm1_out[0]
print(f"Connecting to {port_name}...")
with mido.open_output(port_name) as out:
    print("Sending MIDI START (0xFA) to run 16-step factory sentence...")
    out.send(mido.Message('start'))
    
    # 16 steps at 110 BPM = 16 * (60 / 110 / 4) = 2.18s per loop
    # Let it play 2 loops (approx 4.5 seconds)
    time.sleep(4.5)
    
    print("Sending MIDI STOP (0xFC)...")
    out.send(mido.Message('stop'))
    print("Sentence playback verified successfully!")
