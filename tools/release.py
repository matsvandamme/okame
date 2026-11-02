#!/usr/bin/env python3
"""Build, sign and publish the firmware release that dongles pick up over the air.

usage: release.py [--publish] [--allow-dirty]

The version comes from the VERSION file ("<name> <number>", e.g. "0.7.1 9"); bump it in a commit first.
Signs "okame:<version>:<size>:<sha256>" with the ECDSA P-256 key in ~/.okame/ota_ecdsa_p256.pem (never in the
repo) and uploads okame.bin + manifest.json to a GitHub release, with the notes from CHANGELOG.md.

Before building it checks that the tree is clean and pushed, that the number is above the latest published
release, and that the tests pass: a mistyped high number would block every later update.
"""
import base64
import hashlib
import json
import os
import pathlib
import subprocess
import sys
import urllib.request

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = pathlib.Path(__file__).resolve().parent.parent
KEY = pathlib.Path.home() / ".okame" / "ota_ecdsa_p256.pem"
REPO = "matsvandamme/okame"
MANIFEST_URL = f"https://github.com/{REPO}/releases/latest/download/manifest.json"

publish = "--publish" in sys.argv
name, version = (ROOT / "VERSION").read_text().split()
version = int(version)


def sh(*cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, check=True, capture_output=True, text=True, **kw).stdout.strip()


def fail(msg):
    sys.exit(f"release.py: {msg}")


# --- guards
if "--allow-dirty" not in sys.argv:
    if sh("git", "status", "--porcelain"):
        fail("the working tree has uncommitted changes (commit them, or --allow-dirty for a local test build)")
    if publish:
        sh("git", "fetch", "-q", "origin")
        if sh("git", "rev-parse", "HEAD") != sh("git", "rev-parse", "origin/main"):
            fail("HEAD is not what origin/main has: push first, so the release tag points at public code")
try:
    with urllib.request.urlopen(MANIFEST_URL, timeout=20) as r:
        latest = json.load(r)
except Exception as e:  # noqa: BLE001
    latest = None
    print(f"(could not read the latest manifest: {e})")
if latest and version <= int(latest["version"]):
    fail(f"VERSION says {version} but the latest release is already {latest['version']} ({latest['name']}): bump VERSION")
if latest and latest["name"] == name:
    fail(f"{name} is already published")

# --- tests
print("running the tests…")
subprocess.check_call(["pio", "test", "-e", "native"], cwd=ROOT, stdout=subprocess.DEVNULL)
subprocess.check_call([sys.executable, "tools/check_routes.py"], cwd=ROOT)

# --- build
out = ROOT / "out" / "release" / name
out.mkdir(parents=True, exist_ok=True)
subprocess.check_call(["pio", "run", "-e", "okame"], cwd=ROOT, env=dict(os.environ, OKAME_RELEASE="1"))
image = (ROOT / ".pio" / "build" / "okame" / "firmware.bin").read_bytes()
(out / "okame.bin").write_bytes(image)

# --- sign
sha = hashlib.sha256(image).hexdigest()
message = f"okame:{version}:{len(image)}:{sha}".encode()
key = serialization.load_pem_private_key(KEY.read_bytes(), password=None)
sig = key.sign(message, ec.ECDSA(hashes.SHA256()))
key.public_key().verify(sig, message, ec.ECDSA(hashes.SHA256()))  # sanity check
manifest = {"version": version, "name": name, "size": len(image), "sha256": sha,
            "sig": base64.b64encode(sig).decode()}
(out / "manifest.json").write_text(json.dumps(manifest, indent=2))
print(json.dumps(manifest, indent=2))


def changelog_notes():
    """The section for this version in CHANGELOG.md, or a one-liner."""
    path = ROOT / "CHANGELOG.md"
    if not path.exists():
        return f"Firmware {name} (OTA version {version})."
    lines, keep = path.read_text().splitlines(), []
    for line in lines:
        if line.startswith("## "):
            if keep:
                break
            if line[3:].strip().startswith(name):
                keep.append(f"Firmware {name} (OTA version {version}).\n")
            continue
        if keep:
            keep.append(line)
    return "\n".join(keep).strip() or f"Firmware {name} (OTA version {version})."


if publish:
    subprocess.check_call(["gh", "release", "create", f"v{name}", str(out / "okame.bin"), str(out / "manifest.json"),
                           "--repo", REPO, "--title", f"Okame {name}", "--notes", changelog_notes()])
    print(f"\nPublished v{name}. Send /update to the Okame bot to install it now, or the egg finds it within a day.")
