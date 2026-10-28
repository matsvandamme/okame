#!/usr/bin/env python3
"""Build, sign and publish a firmware release that dongles pick up over the air.

usage: release.py VERSION_NUMBER NAME [--publish]
  e.g. release.py 2 0.1.1 --publish

Signs "okame:<version>:<size>:<sha256>" with the ECDSA P-256 key in ~/.okame/ota_ecdsa_p256.pem
(never in the repo) and uploads okame.bin + manifest.json to a GitHub release.
"""
import base64
import hashlib
import json
import os
import pathlib
import subprocess
import sys

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = pathlib.Path(__file__).resolve().parent.parent
KEY = pathlib.Path.home() / ".okame" / "ota_ecdsa_p256.pem"
REPO = "matsvandamme/okame"

version, name = int(sys.argv[1]), sys.argv[2]
publish = "--publish" in sys.argv
out = ROOT / "out" / "release" / name
out.mkdir(parents=True, exist_ok=True)

env = dict(os.environ, PLATFORMIO_BUILD_FLAGS=f'-DFW_VERSION={version} -DFW_NAME=\\"{name}\\"')
subprocess.check_call(["pio", "run", "-e", "okame"], cwd=ROOT, env=env)
image = (ROOT / ".pio" / "build" / "okame" / "firmware.bin").read_bytes()
(out / "okame.bin").write_bytes(image)

sha = hashlib.sha256(image).hexdigest()
message = f"okame:{version}:{len(image)}:{sha}".encode()
key = serialization.load_pem_private_key(KEY.read_bytes(), password=None)
sig = key.sign(message, ec.ECDSA(hashes.SHA256()))
key.public_key().verify(sig, message, ec.ECDSA(hashes.SHA256()))  # sanity check
manifest = {"version": version, "name": name, "size": len(image), "sha256": sha,
            "sig": base64.b64encode(sig).decode()}
(out / "manifest.json").write_text(json.dumps(manifest, indent=2))
print(json.dumps(manifest, indent=2))

if publish:
    subprocess.check_call(["gh", "release", "create", f"v{name}", str(out / "okame.bin"), str(out / "manifest.json"),
                           "--repo", REPO, "--title", f"Okame {name}", "--notes", f"Firmware {name} (OTA version {version})."])
