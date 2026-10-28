#!/usr/bin/env python3
"""Pack the print-oriented parts into a Bambu Studio 3MF with AMS filament assignments.

Object 1: egg_bowl (filament 1, yellow). Object 2: egg_lid (filament 1) + bezel inlay (filament 2, black) as one
two-colour object, so the inlay prints flush in the same job. Writes case/out/okame_case.3mf.
"""
import pathlib
import zipfile

import trimesh

OUT = pathlib.Path(__file__).resolve().parent.parent / "case" / "out"


def mesh_xml(oid, m):
    v = "".join(f'<vertex x="{x:.4f}" y="{y:.4f}" z="{z:.4f}"/>' for x, y, z in m.vertices)
    t = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in m.faces)
    return f'<object id="{oid}" type="model"><mesh><vertices>{v}</vertices><triangles>{t}</triangles></mesh></object>'


bowl = trimesh.load(OUT / "print_egg_bowl.stl")
lid = trimesh.load(OUT / "print_egg_lid.stl")
inlay = trimesh.load(OUT / "print_bezel_inlay.stl")
# Lay the two plates side by side on a 256 mm bed, each centred on its own spot.
for m, x in ((bowl, 70), (lid, 170)):
    c = m.bounds.mean(axis=0)
    shift = [x - c[0], 128 - c[1], 0]
    m.apply_translation(shift)
    if m is lid:
        inlay.apply_translation(shift)

model = ('<?xml version="1.0" encoding="UTF-8"?>'
         '<model unit="millimeter" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
         '<metadata name="Application">okame make_3mf.py</metadata><resources>'
         + mesh_xml(1, bowl) + mesh_xml(2, lid) + mesh_xml(3, inlay)
         + '<object id="4" type="model"><components><component objectid="1"/></components></object>'
         '<object id="5" type="model"><components><component objectid="2"/><component objectid="3"/></components></object>'
         '</resources><build><item objectid="4"/><item objectid="5"/></build></model>')
settings = ('<?xml version="1.0" encoding="UTF-8"?><config>'
            '<object id="4"><metadata key="name" value="Okame bowl (yellow)"/><metadata key="extruder" value="1"/>'
            '<part id="1" subtype="normal_part"><metadata key="name" value="bowl"/><metadata key="extruder" value="1"/></part></object>'
            '<object id="5"><metadata key="name" value="Okame lid (yellow) + bezel (black)"/><metadata key="extruder" value="1"/>'
            '<part id="2" subtype="normal_part"><metadata key="name" value="lid"/><metadata key="extruder" value="1"/></part>'
            '<part id="3" subtype="normal_part"><metadata key="name" value="bezel inlay"/><metadata key="extruder" value="2"/></part>'
            '</object></config>')
types = ('<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
         '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
         '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>'
         '<Default Extension="config" ContentType="text/xml"/></Types>')
rels = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
        '<Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>'
        '</Relationships>')
path = OUT / "okame_case.3mf"
with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", types)
    z.writestr("_rels/.rels", rels)
    z.writestr("3D/3dmodel.model", model)
    z.writestr("Metadata/model_settings.config", settings)
print(path, path.stat().st_size // 1024, "KB")

# Fit-test job: the flat-bottomed top band of the bowl + the real lid and inlay (~15 minutes).
band = trimesh.load(OUT / "fit_test.stl")
band.apply_translation([70 - band.bounds.mean(axis=0)[0], 128 - band.bounds.mean(axis=0)[1], -band.bounds[0][2]])
model2 = model.replace(mesh_xml(1, bowl), mesh_xml(1, band))
path2 = OUT / "okame_fit_test.3mf"
with zipfile.ZipFile(path2, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("[Content_Types].xml", types)
    z.writestr("_rels/.rels", rels)
    z.writestr("3D/3dmodel.model", model2)
    z.writestr("Metadata/model_settings.config", settings.replace("Okame bowl (yellow)", "Fit test band (yellow)"))
print(path2, path2.stat().st_size // 1024, "KB")
