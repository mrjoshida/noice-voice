#!/usr/bin/env python3
"""Spectrograms of WAV files into one PNG grid, for eyeballing renders (formant motion, hums, noise).

  python3 tools/noice/spectrogram.py build/tract_demo/*.wav --out build/tract_demo/spectrograms.png
"""
import argparse
import wave
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
import numpy as np  # noqa: E402
from scipy.signal import spectrogram  # noqa: E402


def read_wav(path):
    with wave.open(str(path)) as w:
        n, ch, sr = w.getnframes(), w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768.0
    return x.reshape(-1, ch)[:, 0], sr


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("wavs", nargs="+", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--fmax", type=float, default=5000.0)
    a = ap.parse_args()
    cols = 2
    rows = (len(a.wavs) + cols - 1) // cols
    fig, axes = plt.subplots(rows, cols, figsize=(7 * cols, 3 * rows), squeeze=False)
    for ax, path in zip(axes.flat, a.wavs):
        x, sr = read_wav(path)
        x = np.append(x[0], x[1:] - 0.95 * x[:-1])          # pre-emphasis: the formants stand out
        f, t, s = spectrogram(x, sr, nperseg=1024, noverlap=896)
        keep = f <= a.fmax
        ax.pcolormesh(t, f[keep], 10 * np.log10(s[keep] + 1e-12), shading="auto", cmap="magma", vmin=-110, vmax=-40)
        ax.set_title(f"{path.stem}   peak {np.max(np.abs(x)):.2f}", fontsize=9)
        ax.set_ylabel("Hz")
    for ax in axes.flat[len(a.wavs):]:
        ax.axis("off")
    fig.tight_layout()
    a.out.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(a.out, dpi=80)
    print(f"wrote {a.out}")


if __name__ == "__main__":
    main()
