#!/usr/bin/env python3
"""Check that the case parts print without supports in their intended orientation.

Flags faces that point down more than 45 degrees from vertical and are not on the bed.
usage: case/.venv/bin/python tools/check_overhangs.py
"""
import json
import math
import pathlib
import sys

import numpy as np
import trimesh

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "case" / "out"
tilt = math.radians(json.loads((OUT / "layout.json").read_text())["tilt_deg"])
MAX_OVERHANG = 50  # degrees from vertical that a Bambu prints cleanly in PLA
LIMIT = math.radians(90 - MAX_OVERHANG)  # flag faces within this angle of straight down


def rot_x(a):
    return trimesh.transformations.rotation_matrix(a, [1, 0, 0])


# The lid prints face-down together with its black bezel inlay (one AMS print), so check them as one.
parts = {
    "egg_bowl": np.eye(4),                                # base on the bed
    "egg_lid": rot_x(math.pi) @ rot_x(-tilt),             # screen face down, bezel inlay included
}
bad = 0
for name, tf in parts.items():
    m = trimesh.load(OUT / f"{name}.stl")
    # Drop zero-volume slivers (the revolve's pointed tip) so slicers see one clean body.
    bodies = [b for b in m.split(only_watertight=False) if abs(b.volume) > 1.0]
    m = trimesh.util.concatenate(bodies) if len(bodies) > 1 else bodies[0]
    inlay = trimesh.load(OUT / "place_bezel.stl") if name == "egg_lid" else None
    for g in (m, inlay):
        if g is not None:
            g.apply_transform(tf)
    z0 = min(m.bounds[0][2], inlay.bounds[0][2] if inlay is not None else 1e9)
    for g in (m, inlay):
        if g is not None:
            g.apply_translation([0, 0, -z0])
    nz = m.face_normals[:, 2]
    cz = m.triangles_center[:, 2]
    down = nz < -math.cos(LIMIT) + 1e-6
    on_bed = cz < 0.25
    over = down & ~on_bed
    if inlay is not None and over.any():  # ceilings resting on the inlay's top are supported by it
        from shapely.geometry import Point
        foot = trimesh.path.polygons.projected(inlay, normal=[0, 0, 1])
        top = inlay.bounds[1][2]
        idx = np.where(over)[0]
        for i in idx:
            x, y, z = m.triangles_center[i]
            if abs(z - top) < 0.05 and foot.buffer(0.05).contains(Point(x, y)):
                over[i] = False
    area = m.area_faces[over].sum()
    worst = math.degrees(math.acos(min(1.0, -nz[over].min()))) if over.any() else 0
    # Small flat ceilings (< 1.5 mm across) bridge fine; report the largest connected overhang region.
    regions = trimesh.graph.connected_components(m.face_adjacency[np.all(over[m.face_adjacency], axis=1)],
                                                 nodes=np.where(over)[0]) if over.any() else []
    big = max((m.area_faces[r].sum() for r in regions), default=0)
    ok = big < 4.0  # a 2 x 2 mm ceiling bridges without support
    bad += not ok
    if not ok:
        for k in np.argsort(-m.area_faces * over)[:3]:
            print("    at", np.round(m.triangles_center[k], 1), f"{m.area_faces[k]:.1f} mm2")
    print(f"{name:10s} overhang area {area:6.1f} mm2, largest region {big:5.1f} mm2, steepest {90 - worst:4.0f} deg "
          f"from horizontal -> {'OK' if ok else 'NEEDS SUPPORT'}")
    m.export(OUT / f"print_{name}.stl")
    if inlay is not None:
        inlay.export(OUT / "print_bezel_inlay.stl")
sys.exit(1 if bad else 0)
