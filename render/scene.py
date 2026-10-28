"""Okame product renders in Blender Cycles.

usage: blender -b -P render/scene.py -- [--shots hero,front,...] [--samples N] [--scale 1.0]
Shots: hero, front, threequarter, back, exploded, screens, turntable
Reads case/out/*.stl, case/out/layout.json and out/screens/*.png; writes render/out/.
"""
import json
import math
import os
import pathlib
import sys

import bpy
from mathutils import Vector

ROOT = pathlib.Path(__file__).resolve().parent.parent
CASE = ROOT / "case" / "out"
SCREENS = ROOT / "out" / "screens"
OUT = ROOT / "render" / "out"
MM = 0.001

args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []


def arg(name, default):
    return args[args.index(name) + 1] if name in args else default


SHOTS = arg("--shots", "hero,front,threequarter,back,exploded,screens").split(",")
SAMPLES = int(arg("--samples", "384"))
SCALE = float(arg("--scale", "1.0"))
layout = json.loads((CASE / "layout.json").read_text())

# ---------------------------------------------------------------- setup


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.engine = "CYCLES"
    prefs = bpy.context.preferences.addons["cycles"].preferences
    prefs.compute_device_type = "METAL"
    prefs.get_devices()
    for d in prefs.devices:
        d.use = True
    sc.cycles.device = "GPU"
    sc.cycles.samples = SAMPLES
    sc.cycles.use_denoising = True
    sc.cycles.use_adaptive_sampling = True
    sc.cycles.max_bounces = 10
    sc.cycles.caustics_reflective = False
    sc.render.film_transparent = False
    sc.view_settings.view_transform = "AgX"
    for look in ("AgX - Medium High Contrast", "Medium High Contrast"):
        try:
            sc.view_settings.look = look
            break
        except TypeError:
            pass
    sc.unit_settings.system = "METRIC"
    return sc


def node_mat(name):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    return m, m.node_tree.nodes, m.node_tree.links, m.node_tree.nodes["Principled BSDF"]


def set_input(bsdf, names, value):
    for n in names if isinstance(names, (list, tuple)) else [names]:
        if n in bsdf.inputs:
            bsdf.inputs[n].default_value = value
            return


def pla(name, rgb, layer_lines=True):
    """Matte PLA with faint 0.2 mm layer lines along Z."""
    m, nodes, links, b = node_mat(name)
    set_input(b, "Base Color", (*rgb, 1))
    set_input(b, "Roughness", 0.42)
    set_input(b, ["Specular IOR Level", "Specular"], 0.45)
    set_input(b, ["Subsurface Weight", "Subsurface"], 0.04)
    if layer_lines:
        tc = nodes.new("ShaderNodeTexCoord")
        wave = nodes.new("ShaderNodeTexWave")
        wave.wave_type = "BANDS"
        wave.bands_direction = "Z"
        wave.inputs["Scale"].default_value = 1 / (0.2 * MM) / 2 / math.pi * 0.3
        wave.inputs["Distortion"].default_value = 0.4
        bump = nodes.new("ShaderNodeBump")
        bump.inputs["Strength"].default_value = 0.12
        bump.inputs["Distance"].default_value = 0.00004
        links.new(tc.outputs["Object"], wave.inputs["Vector"])
        links.new(wave.outputs["Fac"], bump.inputs["Height"])
        links.new(bump.outputs["Normal"], b.inputs["Normal"])
    return m


def simple(name, rgb, rough=0.5, metal=0.0, coat=0.0):
    m, nodes, links, b = node_mat(name)
    set_input(b, "Base Color", (*rgb, 1))
    set_input(b, "Roughness", rough)
    set_input(b, "Metallic", metal)
    set_input(b, ["Coat Weight", "Clearcoat"], coat)
    return m


def lcd_material(png):
    m, nodes, links, b = node_mat("LCD")
    img = nodes.new("ShaderNodeTexImage")
    img.image = bpy.data.images.load(str(png))
    img.interpolation = "Closest"
    set_input(b, "Roughness", 0.08)
    set_input(b, ["Coat Weight", "Clearcoat"], 1.0)
    links.new(img.outputs["Color"], b.inputs["Base Color"])
    links.new(img.outputs["Color"], b.inputs["Emission Color"] if "Emission Color" in b.inputs else b.inputs["Emission"])
    set_input(b, "Emission Strength", 1.6)
    return m, img


def wood():
    m, nodes, links, b = node_mat("Oak")
    tc = nodes.new("ShaderNodeTexCoord")
    mp = nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (1.0, 14.0, 1.0)
    noise = nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 6.0
    noise.inputs["Detail"].default_value = 10.0
    wave = nodes.new("ShaderNodeTexWave")
    wave.inputs["Scale"].default_value = 3.0
    wave.inputs["Distortion"].default_value = 6.0
    wave.inputs["Detail"].default_value = 4.0
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.33, 0.19, 0.09, 1)
    ramp.color_ramp.elements[1].color = (0.62, 0.42, 0.24, 1)
    bump = nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 0.08
    links.new(tc.outputs["Object"], mp.inputs["Vector"])
    links.new(mp.outputs["Vector"], noise.inputs["Vector"])
    links.new(noise.outputs["Color"], wave.inputs["Vector"])
    links.new(wave.outputs["Fac"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], b.inputs["Base Color"])
    links.new(wave.outputs["Fac"], bump.inputs["Height"])
    links.new(bump.outputs["Normal"], b.inputs["Normal"])
    set_input(b, "Roughness", 0.38)
    set_input(b, ["Coat Weight", "Clearcoat"], 0.25)
    return m


def import_stl(name, mat, smooth=True):
    path = CASE / f"{name}.stl"
    bpy.ops.wm.stl_import(filepath=str(path), global_scale=MM)
    ob = bpy.context.selected_objects[0]
    ob.name = name
    ob.data.materials.append(mat)
    if smooth:
        try:
            bpy.ops.object.shade_auto_smooth(angle=math.radians(35))
        except Exception:
            bpy.ops.object.shade_smooth()
    return ob


def rounded_box(name, size, bevel, mat, loc=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc)
    ob = bpy.context.object
    ob.name = name
    ob.scale = size
    bpy.ops.object.transform_apply(scale=True)
    mod = ob.modifiers.new("bevel", "BEVEL")
    mod.width = bevel
    mod.segments = 6
    mod.limit_method = "ANGLE"
    bpy.ops.object.shade_smooth()
    ob.data.materials.append(mat)
    return ob


def area_light(name, loc, target, size, power, color=(1, 0.96, 0.9)):
    bpy.ops.object.light_add(type="AREA", location=loc)
    lt = bpy.context.object
    lt.name = name
    lt.data.size = size
    lt.data.energy = power
    lt.data.color = color
    look_at(lt, target)
    return lt


def look_at(ob, target):
    d = Vector(target) - ob.location
    ob.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()


def camera(name, loc, target, lens=50, fstop=None, focus=None):
    bpy.ops.object.camera_add(location=loc)
    cam = bpy.context.object
    cam.name = name
    cam.data.lens = lens
    look_at(cam, target)
    if fstop:
        cam.data.dof.use_dof = True
        cam.data.dof.aperture_fstop = fstop
        cam.data.dof.focus_distance = (Vector(focus or target) - cam.location).length
    return cam


def world(color, strength):
    w = bpy.data.worlds.new("World")
    w.use_nodes = True
    bg = w.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (*color, 1)
    bg.inputs["Strength"].default_value = strength
    bpy.context.scene.world = w


# ---------------------------------------------------------------- the egg


def build_egg(png):
    yellow = pla("PLA yellow", (0.98, 0.62, 0.02))
    black = pla("PLA black", (0.012, 0.012, 0.012))
    parts = {
        "egg_bottom": import_stl("egg_bottom", yellow),
        "egg_top": import_stl("egg_top", yellow),
        "place_bezel": import_stl("place_bezel", black),
    }
    # The LCD itself, a little below the bezel, tilted like the dongle.
    lcd_mat, img = lcd_material(png)
    lx, ly, lz = layout["lcd"]
    t = math.radians(layout["tilt_deg"])
    up = 0.9  # mm above the dongle envelope's top face, along the facet normal
    bpy.ops.mesh.primitive_plane_add(size=1, location=(lx * MM, (ly - up * math.sin(t)) * MM, (lz + up * math.cos(t)) * MM))
    lcd = bpy.context.object
    lcd.name = "lcd"
    lcd.scale = (layout["lcd_w"] * MM, layout["lcd_h"] * MM, 1)
    lcd.rotation_euler = (math.radians(layout["tilt_deg"]), 0, 0)
    lcd.data.materials.append(lcd_mat)
    parts["lcd"] = lcd
    # The dongle body (seen through the window edges) and its metal USB-A plug sticking out of the narrow end.
    dongle = import_stl("ref_dongle", simple("Dongle", (0.02, 0.02, 0.025), 0.35), smooth=False)
    parts["dongle"] = dongle
    tip = Vector(layout["plug"]["tip"]) * MM
    pl = layout["plug"]
    plug = rounded_box("usb_plug", (pl["len"] * MM, pl["w"] * MM, pl["h"] * MM), 0.4 * MM,
                       simple("Steel", (0.75, 0.75, 0.76), 0.25, 1.0), loc=tip - Vector((pl["len"] / 2 * MM, 0, 0)))
    plug.rotation_euler = (math.radians(layout["tilt_deg"]), 0, 0)
    parts["plug"] = plug
    root = bpy.data.objects.new("egg_root", None)
    bpy.context.collection.objects.link(root)
    for ob in parts.values():
        ob.parent = root
    return root, parts, img


def cable(points, radius, mat):
    cu = bpy.data.curves.new("cable", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = radius
    cu.bevel_resolution = 6
    sp = cu.splines.new("BEZIER")
    sp.bezier_points.add(len(points) - 1)
    for bp, p in zip(sp.bezier_points, points):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    ob = bpy.data.objects.new("cable", cu)
    bpy.context.collection.objects.link(ob)
    ob.data.materials.append(mat)
    return ob


# ---------------------------------------------------------------- laptop


def build_laptop(origin):
    """An unbranded thin laptop with MacBook-Air-like proportions (304 x 215 x 11 mm)."""
    ox, oy = origin
    alu = simple("Aluminium", (0.80, 0.80, 0.82), rough=0.28, metal=1.0)
    keys_mat = simple("Keys", (0.02, 0.02, 0.022), rough=0.55)
    glass = simple("Screen", (0.004, 0.004, 0.006), rough=0.04, coat=1.0)
    W, D, T = 304 * MM, 215 * MM, 11.3 * MM
    base = rounded_box("laptop_base", (W, D, T * 0.45), 4 * MM, alu, loc=(ox, oy, T * 0.225))
    # Keyboard well and keys.
    kw, kd = 270 * MM, 100 * MM
    ky = oy + D / 2 - 30 * MM - kd / 2
    for r in range(6):
        for c in range(14):
            w = 15.5 * MM if r else 15.5 * MM
            x = ox - kw / 2 + (c + 0.5) * (kw / 14)
            y = ky + kd / 2 - (r + 0.5) * (kd / 6)
            k = rounded_box(f"key_{r}_{c}", (w * 0.9, kd / 6 * 0.82, 0.8 * MM), 1.2 * MM, keys_mat,
                            loc=(x, y, T * 0.45 + 0.2 * MM))
            k.parent = base
    # Trackpad.
    tp = rounded_box("trackpad", (120 * MM, 75 * MM, 0.3 * MM), 3 * MM, simple("Trackpad", (0.72, 0.72, 0.74), 0.22, 1.0),
                     loc=(ox, oy - D / 2 + 50 * MM, T * 0.45))
    tp.parent = base
    # Lid, open about 110 degrees, hinged at the back edge.
    hinge = Vector((ox, oy + D / 2 - 3 * MM, T * 0.45))
    lid = rounded_box("laptop_lid", (W, D, 4 * MM), 3.5 * MM, alu, loc=(0, D / 2, 2 * MM))
    scr = rounded_box("screen", (W - 12 * MM, D - 14 * MM, 0.4 * MM), 2 * MM, glass, loc=(0, D / 2, 4.1 * MM))
    scr.parent = lid
    pivot = bpy.data.objects.new("lid_pivot", None)
    bpy.context.collection.objects.link(pivot)
    pivot.location = hinge
    lid.parent = pivot
    pivot.rotation_euler = (math.radians(180 - 110), 0, 0)  # lid opened to 110 degrees
    # Two USB-C ports on the left side.
    port_mat = simple("Port", (0.01, 0.01, 0.01), 0.6)
    ports = []
    for i, py in enumerate((oy + D / 2 - 40 * MM, oy + D / 2 - 55 * MM)):
        p = rounded_box(f"port_{i}", (1.0 * MM, 8.6 * MM, 2.8 * MM), 1.2 * MM, port_mat,
                        loc=(ox - W / 2 - 0.3 * MM, py, T * 0.25))
        ports.append(p)
    return base, ports


# ---------------------------------------------------------------- scenes


def scene_desk(png):
    sc = reset()
    world((0.55, 0.62, 0.75), 0.18)
    bpy.ops.mesh.primitive_plane_add(size=3, location=(0, 0, 0))
    desk = bpy.context.object
    desk.name = "desk"
    desk.data.materials.append(wood())
    root, parts, img = build_egg(png)
    # Laptop to the right of the egg; the egg's cable end (+X) faces it.
    lap_x = 0.215
    base, ports = build_laptop((lap_x, 0.035))
    root.location = (-0.03, -0.05, 0)
    root.rotation_euler = (0, 0, math.radians(-8))
    bpy.context.view_layer.update()  # make root.matrix_world current
    # The cable's USB-A socket plugs onto the egg's plug; the cable runs over the desk to the laptop.
    white = simple("Cable", (0.92, 0.92, 0.9), 0.45)
    tip = root.matrix_world @ (Vector(layout["plug"]["tip"]) * MM)
    fwd = (root.matrix_world.to_3x3() @ Vector((1, 0, 0))).normalized()
    sock_len = 30 * MM
    sock = rounded_box("usb_socket", (sock_len, 18 * MM, 9 * MM), 2.5 * MM, white,
                       loc=tip + fwd * (sock_len / 2 - 9 * MM))
    sock.rotation_euler = (0, 0, root.rotation_euler.z)
    start = tip + fwd * (sock_len - 9 * MM + 2 * MM)
    port = ports[1].location
    floor_z = 1.7 * MM
    cable([start, start + fwd * 0.012 + Vector((0, 0, -0.006)), Vector((start.x + 0.03, start.y + 0.01, floor_z)),
           Vector((port.x - 0.05, port.y - 0.012, floor_z)), Vector((port.x - 0.02, port.y, port.z - 0.001)),
           Vector((port.x - 0.014, port.y, port.z))], 1.6 * MM, white)
    rounded_box("usbc_plug", (14 * MM, 7.5 * MM, 4.5 * MM), 1.5 * MM, white, loc=(port.x - 7.5 * MM, port.y, port.z))
    # Window light from the back left, soft fill from the front right.
    area_light("window", (-0.55, 0.45, 0.6), (0.05, 0, 0.02), 1.2, 38, (1.0, 0.95, 0.88))
    area_light("fill", (0.5, -0.6, 0.35), (0, 0, 0.02), 0.8, 8, (0.9, 0.95, 1.0))
    area_light("rim", (0.1, 0.6, 0.25), (0, 0, 0.03), 0.3, 5)
    return sc, root, parts, img


def scene_studio(png):
    sc = reset()
    world((0.95, 0.92, 0.85), 0.05)
    # Seamless cream backdrop that curves up behind the egg.
    bpy.ops.mesh.primitive_plane_add(size=4, location=(0, 0.6, 0))
    floor = bpy.context.object
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.subdivide(number_cuts=40)
    bpy.ops.object.mode_set(mode="OBJECT")
    for v in floor.data.vertices:
        y = v.co.y
        if y > -0.3:
            v.co.z = ((y + 0.3) ** 2) * 1.2
    bpy.ops.object.shade_smooth()
    floor.data.materials.append(simple("Backdrop", (0.93, 0.80, 0.52), 0.85))
    root, parts, img = build_egg(png)
    area_light("key", (-0.35, -0.4, 0.45), (0, 0, 0.02), 0.6, 11, (1.0, 0.96, 0.9))
    area_light("top", (0.05, 0.1, 0.6), (0, 0, 0.02), 0.7, 5)
    area_light("rim", (0.35, 0.35, 0.2), (0, 0, 0.03), 0.3, 6, (0.95, 0.97, 1.0))
    return sc, root, parts, img


def render(sc, cam, path, res):
    sc.camera = cam
    sc.render.resolution_x = int(res[0] * SCALE)
    sc.render.resolution_y = int(res[1] * SCALE)
    sc.render.filepath = str(path)
    sc.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    print(f"rendered {path}")


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    adult = SCREENS / "13_main_adult_pearl.png"
    egg_c = (0, 0, 0.017)

    if "hero" in SHOTS:
        sc, root, parts, img = scene_desk(adult)
        cam = camera("hero", (-0.24, -0.36, 0.2), (0.05, 0.02, 0.03), lens=50, fstop=4.0, focus=(-0.03, -0.05, 0.02))
        render(sc, cam, OUT / "hero_desk.png", (1920, 1080))
        cam2 = camera("hero_close", (-0.1, -0.17, 0.1), (-0.015, -0.04, 0.02), lens=70, fstop=4.0,
                      focus=(-0.03, -0.05, 0.025))
        render(sc, cam2, OUT / "hero_close.png", (1600, 1200))

    studio = [s for s in SHOTS if s in ("front", "threequarter", "back", "exploded", "screens", "turntable")]
    if studio:
        sc, root, parts, img = scene_studio(adult)
        views = {
            "front": ((0, -0.15, 0.06), 85),
            "threequarter": ((-0.1, -0.11, 0.075), 85),
            "back": ((0.09, 0.12, 0.06), 85),
        }
        for name, (loc, lens) in views.items():
            if name in SHOTS:
                render(sc, camera(name, loc, egg_c, lens=lens, fstop=5.6), OUT / f"studio_{name}.png", (1600, 1600))
        if "exploded" in SHOTS:
            parts["egg_top"].location.z += 0.03
            parts["place_bezel"].location.z += 0.05
            parts["lcd"].location.z += 0.012
            parts["dongle"].location.z += 0.012
            parts["plug"].location.z += 0.012
            render(sc, camera("exploded", (-0.12, -0.13, 0.11), (0, 0, 0.03), lens=70, fstop=8),
                   OUT / "studio_exploded.png", (1600, 1600))
            for k, dz in (("egg_top", 0.03), ("place_bezel", 0.05), ("lcd", 0.012), ("dongle", 0.012), ("plug", 0.012)):
                parts[k].location.z -= dz
        if "screens" in SHOTS:
            cam = camera("screens", (-0.02, -0.11, 0.09), (0.0, -0.004, 0.028), lens=100, fstop=5.6)
            for shot in ("01_egg", "03_hatch_chick", "11_main_juvenile", "13_main_adult_pearl", "80_sleep_offline",
                         "81_setup_ssid", "59_anim_win", "24_lights_off"):
                img.image = bpy.data.images.load(str(SCREENS / f"{shot}.png"))
                render(sc, cam, OUT / f"screen_{shot}.png", (1200, 900))
        if "turntable" in SHOTS:
            cam = camera("turntable", (0, -0.16, 0.07), egg_c, lens=85, fstop=8)
            sc.camera = cam
            sc.render.resolution_x, sc.render.resolution_y = int(1080 * SCALE), int(1080 * SCALE)
            frames = int(arg("--frames", "144"))
            tdir = OUT / "turntable"
            tdir.mkdir(exist_ok=True)
            for f in range(frames):
                root.rotation_euler = (0, 0, 2 * math.pi * f / frames)
                sc.render.filepath = str(tdir / f"f{f:04d}.png")
                bpy.ops.render.render(write_still=True)
            print("turntable frames done")


main()
