#!/usr/bin/env python3
"""Parse Hershey JHF font into clean stroke polylines."""

def parse_jhf(filename):
    glyphs = []
    with open(filename, 'r') as f:
        for idx, line in enumerate(f):
            if len(line) < 10:
                continue
            gid = int(line[0:5].strip())
            npairs = int(line[5:8].strip())
            lm = ord(line[8]) - ord('R')
            rm = ord(line[9]) - ord('R')
            width = rm - lm
            
            raw_coords = line[10:].rstrip('\r\n')
            strokes = []
            curr_stroke = []
            i = 0
            while i < len(raw_coords) - 1:
                p1 = raw_coords[i]
                p2 = raw_coords[i+1]
                i += 2
                if p1 == ' ' and p2 == 'R':
                    if curr_stroke:
                        strokes.append(curr_stroke)
                        curr_stroke = []
                else:
                    x = ord(p1) - ord('R') - lm
                    y = ord(p2) - ord('R')
                    curr_stroke.append((x, y))
            if curr_stroke:
                strokes.append(curr_stroke)
            
            ascii_code = 32 + idx
            char = chr(ascii_code) if 32 <= ascii_code <= 126 else '?'
            glyphs.append({
                'ascii': ascii_code,
                'char': char,
                'width': width,
                'strokes': strokes
            })
    return glyphs

glyphs = parse_jhf("assets/noice/hershey_simplex.jhf")
print(f"Parsed {len(glyphs)} glyphs.")
for g in glyphs:
    if g['char'] in "A0a":
        print(f"Char '{g['char']}': width {g['width']}, strokes: {g['strokes']}")
