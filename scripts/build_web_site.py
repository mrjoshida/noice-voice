#!/usr/bin/env python3
"""Build unified build/site/ directory for NOICE VOICE GitHub Pages."""
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SIM_SRC = REPO_ROOT / "web" / "sim.html"
SITE_DIR = REPO_ROOT / "build" / "site"


def main():
    if not SIM_SRC.is_file() or SIM_SRC.stat().st_size == 0:
        print(f"ERROR: Missing or empty simulator source: {SIM_SRC}")
        sys.exit(1)

    if SITE_DIR.exists():
        shutil.rmtree(SITE_DIR)
    SITE_DIR.mkdir(parents=True, exist_ok=True)

    index_dst = SITE_DIR / "index.html"
    shutil.copy2(SIM_SRC, index_dst)
    (SITE_DIR / ".nojekyll").write_text("", encoding="utf-8")

    html = index_dst.read_text(encoding="utf-8")
    if "NOICE VOICE" not in html:
        print("ERROR: Built index.html does not look like NOICE VOICE simulator")
        sys.exit(1)

    print(f"Built site at {SITE_DIR.relative_to(REPO_ROOT)}/ (index.html: {index_dst.stat().st_size} B)")


if __name__ == "__main__":
    main()
