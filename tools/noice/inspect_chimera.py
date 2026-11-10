#!/usr/bin/env python3
"""Inspect Chimera vowelTargets and consonantTargets."""
import json
from pathlib import Path

CHIMERA_DIR = Path("/Users/joshcaldwell/.gemini/antigravity/brain/bb9e60fe-1e8c-40b2-a40d-59ed54fc92d4/scratch/chimera/repo")
vowels_file = CHIMERA_DIR / "crates/chimera/src/phonetics/vowelTargets.generated.json"
consonants_file = CHIMERA_DIR / "crates/chimera/src/phonetics/consonantTargets.generated.json"

with open(vowels_file) as f:
    vowels = json.load(f)

print(f"Bodies in vowelTargets: {list(vowels.keys())}")
male_vowels = list(vowels["male"].keys())
print(f"Male vowels count: {len(male_vowels)}")
print(f"Male vowels: {male_vowels}")

with open(consonants_file) as f:
    consonants = json.load(f)

print(f"\nBodies in consonantTargets: {list(consonants.keys())}")
print(f"Consonants: {list(consonants['male'].keys())}")
