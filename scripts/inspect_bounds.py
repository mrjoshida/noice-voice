#!/usr/bin/env python3
import json
from pathlib import Path

path = Path("build/gen/chimera_geometry.json")
with open(path) as f:
    data = json.load(f)

male = data['bodies']['male']
all_pts = []
all_pts.extend(male['skull_outer'])
all_pts.extend(male['palate'])
for v in male['vowels'].values():
    all_pts.extend(v['tongue'])
    all_pts.extend(v['jaw_bone'])
    all_pts.extend(v['upper_lip'])
    all_pts.extend(v['lower_lip'])
    all_pts.extend(v['velum'])
    for r in v['rays']:
        all_pts.append(r['inner'])
        all_pts.append(r['outer'])

xs = [p['x'] for p in all_pts]
ys = [p['y'] for p in all_pts]
print(f"X bounds: min {min(xs):.2f}, max {max(xs):.2f}, span {max(xs)-min(xs):.2f}")
print(f"Y bounds: min {min(ys):.2f}, max {max(ys):.2f}, span {max(ys)-min(ys):.2f}")
