#!/usr/bin/env python3
"""Stage, verify, and deploy NOICE VOICE stock Felucca WebAssembly simulator to GitHub Pages (gh-pages)."""
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
REMOTE_SSH = "git@github.com:mrjoshida/noice-voice.git"
REPO_NAME = "mrjoshida/noice-voice"


def run_cmd(cmd, cwd=None, env=None, check=True):
    print(f"Running: {' '.join(str(c) for c in cmd)} (cwd: {cwd or '.'})")
    res = subprocess.run(cmd, cwd=cwd, env=env, capture_output=True, text=True)
    if check and res.returncode != 0:
        print(f"Command failed ({res.returncode}):\nSTDOUT: {res.stdout}\nSTDERR: {res.stderr}")
        sys.exit(res.returncode)
    return res


def get_git_identity():
    name = run_cmd(["git", "config", "user.name"], cwd=REPO_ROOT, check=False).stdout.strip() or "Josh Caldwell"
    email = run_cmd(["git", "config", "user.email"], cwd=REPO_ROOT, check=False).stdout.strip() or "jpcaldwell@gmail.com"
    return name, email


def main():
    print("=== Step 1: Build Stock WASM Simulator & Stage Site ===")
    run_cmd([sys.executable, "scripts/build_web_site.py"], cwd=REPO_ROOT)

    print("\n=== Step 2: Run Native Parity & Emulator Automated Tests ===")
    (REPO_ROOT / "build" / "host").mkdir(parents=True, exist_ok=True)
    run_cmd([
        "cc", "-O2", "-ffp-contract=off", "-w",
        "-Ifirmware/hal", "-Ifirmware/bsp/chip", "-Ifirmware/bsp/boards/fm1",
        "-Ibuild/gen", "-Ifirmware/src",
        "-o", "build/host/emu_native", "web/emu/native_check.c", "-lm"
    ], cwd=REPO_ROOT)
    run_cmd(["build/host/emu_native", "build/host/emu_native.f32"], cwd=REPO_ROOT)
    res = run_cmd(["node", "web/emu/emu_test.mjs", "build/emu/felucca.wasm", "build/host/emu_native.f32"], cwd=REPO_ROOT)
    print(res.stdout)

    site_dir = REPO_ROOT / "build" / "site"
    required = [
        site_dir / "index.html",
        site_dir / "felucca.wasm",
        site_dir / "worklet.js",
        site_dir / "fonts" / "DotGothic16-subset.woff",
        site_dir / "fonts" / "OFL.txt",
        site_dir / ".nojekyll",
    ]
    for req in required:
        if not req.exists():
            print(f"ERROR: Missing required staged file: {req}")
            sys.exit(1)

    print("\n=== Step 3: Push gh-pages branch ===")
    git_name, git_email = get_git_identity()
    with tempfile.TemporaryDirectory(prefix="noice_gh_pages_") as tmpdir:
        tmp_path = Path(tmpdir)
        for item in site_dir.iterdir():
            if item.is_dir():
                shutil.copytree(item, tmp_path / item.name)
            else:
                shutil.copy2(item, tmp_path / item.name)

        run_cmd(["git", "init", "-b", "gh-pages"], cwd=tmp_path)
        run_cmd(["git", "config", "user.name", git_name], cwd=tmp_path)
        run_cmd(["git", "config", "user.email", git_email], cwd=tmp_path)
        run_cmd(["git", "remote", "add", "origin", REMOTE_SSH], cwd=tmp_path)
        run_cmd(["git", "add", "-A"], cwd=tmp_path)
        run_cmd(["git", "commit", "-m", "Deploy NOICE VOICE stock WASM simulator to GitHub Pages"], cwd=tmp_path)
        run_cmd(["git", "push", "-f", "origin", "gh-pages"], cwd=tmp_path)

    print("\n=== Step 4: Enable / Configure GitHub Pages ===")
    env = os.environ.copy()
    chk = run_cmd(["gh", "api", f"repos/{REPO_NAME}/pages"], env=env, check=False)
    if chk.returncode == 0:
        upd = run_cmd([
            "gh", "api", "-X", "PUT", f"repos/{REPO_NAME}/pages",
            "-f", "source[branch]=gh-pages",
            "-f", "source[path]=/"
        ], env=env, check=False)
        print(f"Pages updated: {upd.stdout.strip() or upd.stderr.strip()}")
    else:
        enable = run_cmd([
            "gh", "api", "-X", "POST", f"repos/{REPO_NAME}/pages",
            "-f", "source[branch]=gh-pages",
            "-f", "source[path]=/"
        ], env=env, check=False)
        print(f"Pages enabled: {enable.stdout.strip() or enable.stderr.strip()}")

    info = run_cmd(["gh", "api", f"repos/{REPO_NAME}/pages"], env=env, check=False)
    if info.returncode == 0:
        print("\nGitHub Pages Info:")
        print(info.stdout)
    else:
        print(f"Warning: could not query pages info: {info.stderr}")


if __name__ == "__main__":
    main()
