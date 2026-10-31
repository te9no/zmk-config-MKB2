#!/usr/bin/env python3
"""Build one pinned recovery target and record verifiable release provenance."""
import argparse
import hashlib
import json
import pathlib
import shutil
import struct
import subprocess
import yaml

p = argparse.ArgumentParser()
p.add_argument("target")
args = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
def output(*cmd):
    return subprocess.check_output(cmd, text=True).strip()
workspace = pathlib.Path(output("west", "topdir"))
zmk = pathlib.Path(output("west", "list", "zmk", "-f", "{abspath}"))
zephyr = pathlib.Path(output("west", "list", "zephyr", "-f", "{abspath}"))
recovery = pathlib.Path(output("west", "list", "zmk-module-bt-recovery", "-f", "{abspath}"))
pin = "7e76381d90856a42e739cebf66752afebbe78748"
assert output("git", "-C", str(recovery), "rev-parse", "HEAD") == pin
targets = {t.get("artifact-name", t["shield"]): t for t in yaml.safe_load((root / "build.yaml").read_text())["include"]}
t = targets[args.target]
reset = t["shield"] == "settings_reset"
subprocess.run(["python3", str(root / "scripts/apply-bt-recovery.py"), str(zmk)], check=True)
build = workspace / "build-bt-recovery" / args.target
snippets = t.get("snippet", "").split()
if not reset:
    snippets.append("bt-recovery")
cmd = ["west", "build", "-s", str(zmk / "app"), "-d", str(build), "-b", t["board"], "-o=-j4"]
if snippets:
    cmd += ["-S", " ".join(snippets)]
cmd += ["--", "-DZMK_CONFIG=" + str(root / "config"), "-DZMK_EXTRA_MODULES=" + str(root),
        "-DZephyr_DIR=" + str(zephyr / "share/zephyr-package/cmake"), "-DSHIELD=" + t["shield"]]
subprocess.run(cmd, check=True)
config = (build / "zephyr/.config").read_text()
assert ("CONFIG_ZMK_BT_RECOVERY=y\n" in config) == (not reset)
if not reset:
    assert "CONFIG_ZMK_BT_RECOVERY_BRIDGE_V1=y\n" in config
    assert ("CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y\n" in config) == args.target.startswith("MKB_L_")
data = (build / "zephyr/zmk.uf2").read_bytes()
assert data and len(data) % 512 == 0
for offset in range(0, len(data), 512):
    assert struct.unpack_from("<II", data, offset) == (0x0A324655, 0x9E5D5157)
    assert struct.unpack_from("<I", data, offset + 508)[0] == 0x0AB16F30
dirty = bool(output("git", "-C", str(root), "status", "--porcelain"))
if dirty:
    raise SystemExit("Refusing release artifacts from a dirty config tree")
dest = workspace / "release-artifacts"
dest.mkdir(exist_ok=True)
name = args.target + ".uf2"
shutil.copyfile(build / "zephyr/zmk.uf2", dest / name)
proof = {
    "inputs": {"source": {"commit": output("git", "-C", str(root), "rev-parse", "HEAD"), "dirty": dirty},
               "zmk": output("git", "-C", str(zmk), "rev-parse", "HEAD"),
               "bridge_sha256": hashlib.sha256((root / "patches/zmk-e5c9b69-bt-recovery.patch").read_bytes()).hexdigest(),
               "recovery_module": pin, "target": t, "snippets": snippets},
    "artifact": {"name": name, "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)},
    "validation": {"recovery_enabled": not reset, "hardware_tested": False}}
(dest / (args.target + ".json")).write_text(json.dumps(proof, indent=2) + "\n")
