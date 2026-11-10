#!/usr/bin/env python3
import os
from pathlib import Path

celery = Path("/Users/joshcaldwell/dev/celery-man")
for root, dirs, files in os.walk(celery):
    for f in files:
        if "hershey" in f.lower() or "vector" in f.lower() or "font" in f.lower():
            print(os.path.join(root, f))
