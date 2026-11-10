#!/usr/bin/env python3
import json
from pathlib import Path

CHIMERA_DIR = Path("/Users/joshcaldwell/.gemini/antigravity/brain/bb9e60fe-1e8c-40b2-a40d-59ed54fc92d4/scratch/chimera/repo")
vowels_file = CHIMERA_DIR / "crates/chimera/src/phonetics/vowelTargets.generated.json"

with open(vowels_file) as f:
    data = json.load(f)

grid_9 = [
    ["i", "ɨ", "u"],
    ["e", "ə", "o"],
    ["æ", "a", "ɑ"]
]

for body in ["male", "female", "child"]:
    print(f"--- {body} ---")
    for r, row in enumerate(grid_9):
        for c, v in enumerate(row):
            if v in data[body]:
                artic = data[body][v]["artic"]
                print(f"({r},{c}) {v}: tbX={artic['tbX']}, tbY={artic['tbY']}, jaw={artic['jaw']}, lipAp={artic['lipAp']}")
            else:
                print(f"({r},{c}) {v}: MISSING!")
