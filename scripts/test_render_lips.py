#!/usr/bin/env python3
"""Inspect and verify upper_lip, lower_lip, and velum rendering."""
import json

with open("build/gen/chimera_geometry.json") as f:
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

v = male["vowels"]["ə"]

u_lip = [(tx(p['x']), ty(p['y'])) for p in v["upper_lip"]]
l_lip = [(tx(p['x']), ty(p['y'])) for p in v["lower_lip"]]
velum = [(tx(p['x']), ty(p['y'])) for p in v["velum"]]
palate = [(tx(p['x']), ty(p['y'])) for p in male["palate"]]

print(f"Palate start: {palate[0]}, end: {palate[-1]}")
print(f"Upper lip start: {u_lip[0]}, end: {u_lip[-1]}")
print(f"Lower lip start: {l_lip[0]}, end: {l_lip[-1]}")
print(f"Velum start: {velum[0]}, end: {velum[-1]}")

print("\nUpper lip points:")
for i, pt in enumerate(u_lip):
    print(f"  {i}: {pt}")

print("\nLower lip points:")
for i, pt in enumerate(l_lip):
    print(f"  {i}: {pt}")
