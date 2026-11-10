#!/usr/bin/env python3
"""Render full midsagittal tract with lips and velum to verify appearance."""
import json
import math

with open("assets/noice/chimera_geometry.json") as f:
    d = json.load(f)

male = d["bodies"]["male"]
grid_3x3 = d["grid_3x3"]

SCALE = 13.5
OX = 45.0
OY = 160.0

def tx(x):
    return int(round(OX + SCALE * x))

def ty(y):
    return int(round(OY - SCALE * y))

# Let's inspect the bounding box of the whole head
skull = [(tx(p['x']), ty(p['y'])) for p in male['skull_outer']]
palate = [(tx(p['x']), ty(p['y'])) for p in male['palate']]
v = male["vowels"]["ə"]
tongue = [(tx(p['x']), ty(p['y'])) for p in v["tongue"]]
jaw = [(tx(p['x']), ty(p['y'])) for p in v["jaw_bone"]]
u_lip = [(tx(p['x']), ty(p['y'])) for p in v["upper_lip"]]
l_lip = [(tx(p['x']), ty(p['y'])) for p in v["lower_lip"]]
velum = [(tx(p['x']), ty(p['y'])) for p in v["velum"]]

all_pts = skull + palate + tongue + jaw + u_lip + l_lip + velum
min_x = min(p[0] for p in all_pts)
max_x = max(p[0] for p in all_pts)
min_y = min(p[1] for p in all_pts)
max_y = max(p[1] for p in all_pts)

print(f"X bounds: {min_x}..{max_x}")
print(f"Y bounds: {min_y}..{max_y}")
print(f"Upper lip: {u_lip}")
print(f"Lower lip: {l_lip}")
