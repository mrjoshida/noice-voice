#!/usr/bin/env python3
"""Extract stroke contours from AVHersheySimplexLight.otf using RecordingPen."""
from pathlib import Path
from fontTools.ttLib import TTFont
from fontTools.pens.recordingPen import RecordingPen

font_path = Path("/Users/joshcaldwell/.gemini/antigravity/brain/bb9e60fe-1e8c-40b2-a40d-59ed54fc92d4/scratch/chimera/repo/public/fonts/AVHersheySimplexLight.otf")
font = TTFont(font_path)
glyph_set = font.getGlyphSet()

for name in ["A", "a", "one", "zero"]:
    pen = RecordingPen()
    glyph_set[name].draw(pen)
    print(f"\n--- Glyph {name} (value count {len(pen.value)}) ---")
    for op, args in pen.value[:8]:
        print(f"  {op} {args}")
