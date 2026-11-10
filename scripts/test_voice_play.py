#!/usr/bin/env python3
import time
import mido

def main():
    print("Available MIDI output ports:")
    outputs = mido.get_output_names()
    print(outputs)

    # Find FM-1 port (or Felucca / NOICE)
    fm1_ports = [p for p in outputs if "FM-1" in p or "Felucca" in p or "NOICE" in p]
    if not fm1_ports:
        print("No FM-1 MIDI port found, using first available or aborting.")
        if outputs:
            fm1_ports = [outputs[0]]
        else:
            return

    port_name = fm1_ports[0]
    print(f"Opening MIDI port: {port_name}")

    with mido.open_output(port_name) as out:
        # Play a vocal scale with vowels: /i/ -> /e/ -> /a/ -> /o/ -> /u/
        notes = [60, 62, 64, 65, 67] # C4, D4, E4, F4, G4
        # CC 20 = TONGUE (P_E0), CC 21 = JAW (P_E1), CC 22 = LIPS (P_E2), CC 23 = VELUM (P_E3)
        # Or standard MIDI CC: CC2 is breath / lung pressure, CC1 is modwheel

        print("Playing vocal phrase...")
        # Note 1: /i/ (tongue front=0, jaw closed=0)
        out.send(mido.Message('control_change', control=20, value=0))
        out.send(mido.Message('control_change', control=21, value=0))
        out.send(mido.Message('note_on', note=60, velocity=100))
        time.sleep(0.4)
        out.send(mido.Message('note_off', note=60))
        time.sleep(0.05)

        # Note 2: /e/ (tongue front=0, jaw mid=64)
        out.send(mido.Message('control_change', control=20, value=0))
        out.send(mido.Message('control_change', control=21, value=64))
        out.send(mido.Message('note_on', note=62, velocity=100))
        time.sleep(0.4)
        out.send(mido.Message('note_off', note=62))
        time.sleep(0.05)

        # Note 3: /a/ (tongue central=64, jaw open=127)
        out.send(mido.Message('control_change', control=20, value=64))
        out.send(mido.Message('control_change', control=21, value=127))
        out.send(mido.Message('note_on', note=64, velocity=100))
        time.sleep(0.4)
        out.send(mido.Message('note_off', note=64))
        time.sleep(0.05)

        # Note 4: /o/ (tongue back=127, jaw mid=64, lips round=50)
        out.send(mido.Message('control_change', control=20, value=127))
        out.send(mido.Message('control_change', control=21, value=64))
        out.send(mido.Message('control_change', control=22, value=50))
        out.send(mido.Message('note_on', note=65, velocity=100))
        time.sleep(0.4)
        out.send(mido.Message('note_off', note=65))
        time.sleep(0.05)

        # Note 5: /u/ (tongue back=127, jaw closed=0, lips round=30)
        out.send(mido.Message('control_change', control=20, value=127))
        out.send(mido.Message('control_change', control=21, value=0))
        out.send(mido.Message('control_change', control=22, value=30))
        out.send(mido.Message('note_on', note=67, velocity=100))
        time.sleep(0.6)
        out.send(mido.Message('note_off', note=67))

        print("Done playing phrase!")

if __name__ == "__main__":
    main()
