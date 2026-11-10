#!/usr/bin/env python3
import urllib.request
import json

# Let's check if the standard Hershey fonts are on github or archive
# Hershey fonts are public domain (US Govt, 1967)
url = "https://raw.githubusercontent.com/kamalmostafa/hershey-fonts/master/hershey-fonts/rowmans.jhf"
try:
    with urllib.request.urlopen(url, timeout=5) as resp:
        content = resp.read().decode('ascii')
        print(f"Downloaded rowmans.jhf, length: {len(content)} bytes")
        with open("assets/noice/hershey_simplex.jhf", "w") as f:
            f.write(content)
except Exception as e:
    print(f"Error fetching Hershey font: {e}")
