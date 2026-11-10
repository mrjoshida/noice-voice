#!/usr/bin/env python3
"""Inspect AVHersheySimplexLight.otf using fontTools."""
from pathlib import Path
from fontTools.ttLib import TTFont

font_path = Path("/Users/joshcaldwell/.gemini/antigravity/brain/bb9e60fe-1e8c-40b2-a40d-59ed54fc92d4/scratch/chimera/repo/public/fonts/AVHersheySimplexLight.otf")
font = TTFont(font_path)

print("Tables in font:", list(font.keys()))
cmap = font.getBestCmap()
print(f"Number of mapped glyphs in cmap: {len(cmap)}")
sample_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-*/=: "
for ch in "A1a":
    code = ord(ch)
    gname = cmap.get(code)
    print(f"Char '{ch}' (0x{code:02x}) -> glyph '{gname}'")
