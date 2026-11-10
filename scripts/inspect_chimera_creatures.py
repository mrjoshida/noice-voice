#!/usr/bin/env python3
import json
from pathlib import Path

chimera_path = Path("/Users/joshcaldwell/.gemini/antigravity/brain/bb9e60fe-1e8c-40b2-a40d-59ed54fc92d4/scratch/chimera/repo")
vowels_file = chimera_path / "crates/chimera/src/phonetics/vowelTargets.generated.json"

if vowels_file.exists():
    with open(vowels_file) as f:
        v = json.load(f)
    print("Creatures / Bodies in Chimera:", list(v.keys()))
    for k in v.keys():
        print(f"  {k}: {len(v[k])} vowels")
else:
    print(f"File not found: {vowels_file}")
