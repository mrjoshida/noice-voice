#!/usr/bin/env python3
"""Build build/site/ directory for NOICE VOICE GitHub Pages using stock Felucca WASM simulator."""
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
GEN_DIR = REPO_ROOT / "build" / "gen"
TOOLS_DIR = REPO_ROOT / "tools"
EMU_DIR = REPO_ROOT / "build" / "emu"
SITE_DIR = REPO_ROOT / "build" / "site"

GENERATORS = [
    (GEN_DIR / "ui_fonts.h", [TOOLS_DIR / "gen_aa_font.py", GEN_DIR / "ui_fonts.h", "--preset", "inter-tight"]),
    (GEN_DIR / "ui_icons.h", [TOOLS_DIR / "gen_aa_icons.py", GEN_DIR / "ui_icons.h"]),
    (GEN_DIR / "ui_keycaps.h", [TOOLS_DIR / "gen_aa_keycaps.py", GEN_DIR / "ui_keycaps.h"]),
    (GEN_DIR / "ui_palettes.h", [TOOLS_DIR / "gen_ui_palettes.py", GEN_DIR / "ui_palettes.h"]),
    (GEN_DIR / "felucca_tables.h", [TOOLS_DIR / "gen_tables.py", GEN_DIR / "felucca_tables.h"]),
    (GEN_DIR / "felucca_fm6.h", [TOOLS_DIR / "gen_fm6_patches.py", GEN_DIR / "felucca_fm6.h"]),
    (GEN_DIR / "felucca_samples.h", [TOOLS_DIR / "gen_samples.py", GEN_DIR / "felucca_samples.h"]),
    (GEN_DIR / "felucca_adpcm.h", [TOOLS_DIR / "gen_adpcm.py", GEN_DIR / "felucca_adpcm.h"]),
    (GEN_DIR / "nv_tables.h", [TOOLS_DIR / "noice" / "gen_tables.py"]),
]

REQUIRED_EMU_FILES = [
    "index.html",
    "felucca.wasm",
    "worklet.js",
    "fonts/DotGothic16-subset.woff",
    "fonts/OFL.txt",
]


def run_cmd(cmd):
    print(f"Running: {' '.join(str(c) for c in cmd)}")
    res = subprocess.run(cmd, cwd=REPO_ROOT)
    if res.returncode != 0:
        sys.exit(res.returncode)


def ensure_generated_headers():
    GEN_DIR.mkdir(parents=True, exist_ok=True)
    missing = [(out, cmd) for out, cmd in GENERATORS if not out.is_file() or out.stat().st_size == 0]
    if not missing:
        print("Generated headers in build/gen/ already present.")
        return
    print(f"Generating {len(missing)} missing header(s) in build/gen/...")
    for out, cmd in missing:
        run_cmd([sys.executable, *(str(p) for p in cmd)])
        if not out.is_file() or out.stat().st_size == 0:
            print(f"ERROR: Generator did not produce {out}")
            sys.exit(1)


def main():
    ensure_generated_headers()

    run_cmd(["sh", str(REPO_ROOT / "web" / "emu" / "build.sh")])

    for rel in REQUIRED_EMU_FILES:
        src = EMU_DIR / rel
        if not src.is_file() or src.stat().st_size == 0:
            print(f"ERROR: Missing or empty emulator build artifact: {src}")
            sys.exit(1)

    if SITE_DIR.exists():
        shutil.rmtree(SITE_DIR)
    SITE_DIR.mkdir(parents=True, exist_ok=True)

    for rel in REQUIRED_EMU_FILES:
        src = EMU_DIR / rel
        dst = SITE_DIR / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)

    (SITE_DIR / ".nojekyll").write_text("", encoding="utf-8")

    for rel in [*REQUIRED_EMU_FILES, ".nojekyll"]:
        dst = SITE_DIR / rel
        if not dst.is_file():
            print(f"ERROR: Missing staged site file: {dst}")
            sys.exit(1)

    wasm_size = (SITE_DIR / "felucca.wasm").stat().st_size
    html_size = (SITE_DIR / "index.html").stat().st_size
    print(f"Built site at {SITE_DIR.relative_to(REPO_ROOT)}/ (index.html: {html_size} B, felucca.wasm: {wasm_size} B)")


if __name__ == "__main__":
    main()
