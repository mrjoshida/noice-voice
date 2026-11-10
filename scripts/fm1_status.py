#!/usr/bin/env python3
"""Query status / run commands on FM-1 CDC console."""
import argparse
import glob
import sys
import time
import serial

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", nargs="?", default="status")
    a = ap.parse_args()
    ports = sorted(glob.glob("/dev/cu.usbmodem*"))
    if not ports:
        sys.exit("No /dev/cu.usbmodem* port found.")
    s = serial.Serial(ports[0], 115200, timeout=0.3)
    s.dtr = True
    time.sleep(0.1)
    s.reset_input_buffer()
    s.write((a.cmd + "\r\n").encode())
    time.sleep(0.4)
    out = s.read(8192).decode("ascii", errors="replace")
    print(out)

if __name__ == "__main__":
    main()
