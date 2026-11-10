#!/usr/bin/env python3
import os
from PIL import Image

def main():
    for name in ["nv_view_neutral", "nv_view_vowel_i"]:
        ppm_path = f"build/test_out/{name}.ppm"
        png_path = f"build/test_out/{name}.png"
        if os.path.exists(ppm_path):
            img = Image.open(ppm_path)
            img.save(png_path)
            print(f"Converted {ppm_path} -> {png_path} ({img.size})")

if __name__ == "__main__":
    main()
