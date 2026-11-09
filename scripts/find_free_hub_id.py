#!/usr/bin/env python3
"""Find FM-1 package identities that The Firmware Hub catalog does not list yet.

  python3 scripts/find_free_hub_id.py FM-1_700 FM-1_7007 ...
Prints the identities the catalog (and its KB page) mention, then each candidate as FREE or TAKEN.
"""
import argparse
import re
import sys
import urllib.error
import urllib.request

HEADERS = {"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"}
SOURCES = ["https://thefirmwarehub.com/catalog.json", "https://thefirmwarehub.com/kb/"]


def fetch(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=HEADERS), timeout=15) as r:
            return r.read().decode("utf-8", errors="replace")
    except (urllib.error.URLError, TimeoutError) as e:
        print(f"warning: {url}: {e}", file=sys.stderr)
        return ""


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("candidates", nargs="+", help="identities to check, e.g. FM-1_700")
    a = ap.parse_args()
    text = "".join(fetch(u) for u in SOURCES)
    if not text:
        sys.exit("no source could be read")
    taken = sorted(set(re.findall(r"FM-1_[A-Za-z0-9]+", text)))
    print("taken:", " ".join(taken))
    for c in a.candidates:
        print(f"{c}: {'TAKEN' if c in taken else 'FREE'}")


if __name__ == "__main__":
    main()
