#!/usr/bin/env python3
from PIL import Image

im = Image.open("build/test_out/nv_view_neutral.png")
w, h = im.size
print(f"Image size: {w}x{h}")

pink_pixels = []
for y in range(h):
    for x in range(w):
        r, g, b = im.getpixel((x, y))[:3]
        # Pink color check: high red, moderate green, high blue
        if r > 200 and b > 140 and g < 150:
            pink_pixels.append((x, y, r, g, b))

print(f"Found {len(pink_pixels)} pink pixels for lips!")
if pink_pixels:
    xs = [p[0] for p in pink_pixels]
    ys = [p[1] for p in pink_pixels]
    print(f"Lips bounding box: X={min(xs)}..{max(xs)}, Y={min(ys)}..{max(ys)}")
    print(f"Sample pink pixels: {pink_pixels[:5]}")
