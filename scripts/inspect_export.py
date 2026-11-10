#!/usr/bin/env python3
import json
from pathlib import Path

path = Path("assets/noice/chimera_geometry.json")
with open(path) as f:
    data = json.load(f)

print(f"Bodies: {list(data['bodies'].keys())}")
print(f"Grid 3x3: {data['grid_3x3']}")
print(f"Onsets ({len(data['onset_list'])}): {data['onset_list']}")
print(f"Codas ({len(data['coda_list'])}): {data['coda_list']}")

male = data['bodies']['male']
print(f"Male vowels ({len(male['vowels'])}): {list(male['vowels'].keys())}")
print(f"Male consonants ({len(male['consonants'])}): {list(male['consonants'].keys())}")
print(f"Skull outer points: {len(male['skull_outer'])}")
print(f"Palate points: {len(male['palate'])}")

# Check areas of /a/ vs /i/
a_areas = male['vowels']['a']['areas_22']
i_areas = male['vowels']['i']['areas_22']
print(f"\n/a/ areas (22 sections): {[round(x, 2) for x in a_areas]}")
print(f"/i/ areas (22 sections): {[round(x, 2) for x in i_areas]}")
