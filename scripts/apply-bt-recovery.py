#!/usr/bin/env python3
"""Explicit bridge installation into an isolated, pinned ZMK checkout."""
import pathlib
import subprocess
import sys
root = pathlib.Path(__file__).resolve().parents[1]
tree = pathlib.Path(sys.argv[1]).resolve()
def git(*args):
    return subprocess.run(["git", "-C", str(tree), *args], capture_output=True, text=True)
if git("rev-parse", "HEAD").stdout.strip() != "e5c9b6915b56801193e359dd9bad4a167ce0d1b8":
    sys.exit("Unsupported ZMK revision; refusing to patch")
patch = str(root / "patches/zmk-e5c9b69-bt-recovery.patch")
if git("apply", "--reverse", "--check", patch).returncode == 0:
    print("Bridge already applied")
else:
    result = git("apply", "--check", patch)
    if result.returncode:
        sys.exit(result.stderr)
    result = git("apply", patch)
    if result.returncode:
        sys.exit(result.stderr)
    print("Applied MeKaBu pinned bridge")
