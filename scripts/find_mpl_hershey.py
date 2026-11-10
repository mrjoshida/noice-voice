#!/usr/bin/env python3
import os
import matplotlib

mpl_dir = os.path.dirname(matplotlib.__file__)
for root, dirs, files in os.walk(mpl_dir):
    for f in files:
        if "hershey" in f.lower():
            print(os.path.join(root, f))
