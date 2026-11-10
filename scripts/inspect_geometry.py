#!/usr/bin/env python3
import json

with open("assets/noice/chimera_geometry.json") as f:
    d = json.load(f)

male = d["bodies"]["male"]
grid_3x3 = d["grid_3x3"]

for r in range(3):
    for c in range(3):
        v_name = grid_3x3[r][c]
        v = male["vowels"][v_name]
        print(f"({r},{c}) /{v_name}/: velum length={len(v['velum'])}")
