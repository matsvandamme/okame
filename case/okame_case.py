"""Okame desk egg: a two-part egg shell around the T-Dongle-S3 clone. Its USB-A plug pokes out of the
narrow end; the cable's socket plugs onto it outside the egg and runs to the laptop.

Print orientation (no supports needed; tools/check_overhangs.py verifies):
  egg_bowl   - flat base on the bed.            (yellow)
  egg_lid    - screen face DOWN on the bed.     (yellow) - gives a perfectly smooth front
  bezel      - front face down.                 (black)
There is no button: the BOOT button sits on the dongle's underside and all interaction is on the phone.
  fit_test   - flat on the bed: a 10-minute slice to check the dongle and socket fit first.

Coordinates: X = egg length (cable exits at +X), Y = depth (user sits at -Y), Z = up, base at Z=0.
Run: case/.venv/bin/python case/okame_case.py   (writes case/out/*.stl)
"""
import math
import pathlib

import cadquery as cq

# ============================================================
# PARAMETERS
# ============================================================
# --- Dongle (T-Dongle-S3 clone). ESTIMATES until measured: confirm before the final print. ---
dongle_len = 46.0        # mm - GNPE Pocket-Dongle-S3 bare board (~34 x 14.6 mm, from listing photos scaled by the 12 mm USB-A plug) + plug
dongle_w = 14.8          # mm
dongle_t = 6.0           # mm - estimate: PCB + LCD module + parts on the back (BOOT button underneath)
plug_len = 12.0          # mm - USB-A plug length
lcd_view_w = 22.0        # mm - visible LCD area (0.96" 160x80 panel is ~21.7 x 10.8 mm)
lcd_view_h = 11.2        # mm
lcd_from_back = 17.0     # mm - LCD centre from the back end (board centre; verify)
button_from_back = 40.0  # mm - BOOT button centre from the back end
button_on_top = True     # button on the screen side (False: on the back/underside)

# --- The USB-A plug that sticks out of the egg (USB-A spec: 12.0 x 4.5 mm). ---
plug_w = 12.0            # mm
plug_h = 4.5             # mm
plug_out = 9.0           # mm - how much of the plug must stay outside the egg to mate with the socket

# --- Egg ---
egg_len = 54.0           # mm - along X
egg_depth = 40.0         # mm - along Y
egg_height = 34.0        # mm - after the flat base is cut
base_cut = 0.30          # fraction of the full egg height cut away to make the flat base
egg_skew = 0.07          # 0 = ellipse; >0 makes the -X end rounder and the +X end narrower (egg-like)
wall = 2.0               # mm
tilt_deg = 15.0          # screen tilt toward the user
face_y = -2.5           # mm - move the facet and dongle toward the user
facet_drop = 10.0        # mm - how far below the apex the screen facet is sliced
assembly_shift = 0.0     # mm - shift the dongle+socket toward the round (-X) end
facet_gap = 1.6          # mm - facet underside to the dongle's top face (room for the button flange)
split_below_mid = 0.0    # mm - split plane relative to the dongle's mid-plane (vertical)
rib_t = 2.4              # mm

# --- Fit & hardware ---
lid_fit = 0.12           # mm - gap per side between the lid's lip and the bowl (snug friction fit)
lip_h = 2.0              # mm
lip_t = 1.2              # mm
clear = 0.4              # mm - clearance around the dongle and socket (PLA on a Bambu)
split_z_frac = 0.55      # where the shell splits, as a fraction of egg_height
screw_d = 1.8            # mm - M2 self-tapping pilot
screw_head_d = 4.4       # mm
foot_d = 8.5             # mm - rubber bumper recess
foot_depth = 1.2         # mm
bezel_lip = 3.0          # mm - bezel border around the window
bezel_t = 1.6            # mm
bezel_recess = 1.0       # mm - black inlay depth (AMS prints it flush with the face)
bezel_r = 3.0            # mm
button_d = 11.0          # mm - big button cap
button_proud = 1.6       # mm
button_flange = 1.0      # mm
boss_pts = [(-9.0, 12.0), (9.0, -12.0)]
boss_r = 2.8              # mm
screw_clear_d = 2.4      # mm
screw_head_h = 2.2       # mm
screw_len = 4.5          # mm - pilot depth in the top (M2 x 6 screws)

OUT = pathlib.Path(__file__).resolve().parent / "out"

# ============================================================
# HELPERS
# ============================================================
full_h = egg_height / (1 - base_cut)
z_center = full_h / 2 - full_h * base_cut  # egg centre height above the bed


def egg_solid(length, depth, height, skew):
    """Revolve an egg profile about X, then squash it to the requested depth/height."""
    a = length / 2
    pts = []
    n = 48
    for i in range(n + 1):
        t = math.pi * i / n
        x = -a * math.cos(t)
        r = (depth / 2) * math.sin(t) * (1 - skew * (x / a))
        pts.append((x, max(r, 0.0)))
    prof = cq.Workplane("XY").spline(pts[1:-1], includeCurrent=False).close()
    prof = cq.Workplane("XY").moveTo(-a, 0).spline(pts[1:-1]).lineTo(a, 0).close()
    solid = prof.revolve(360, (-a, 0, 0), (a, 0, 0)).val()
    m = cq.Matrix()
    gt = __import__("OCP.gp", fromlist=["gp_GTrsf"]).gp_GTrsf()
    gt.SetValue(3, 3, height / depth)
    from OCP.BRepBuilderAPI import BRepBuilderAPI_GTransform

    shape = BRepBuilderAPI_GTransform(solid.wrapped, gt, True).Shape()
    del m
    return cq.Workplane("XY").add(cq.Shape.cast(shape))


def tilted(wp_obj, origin):
    """Rotate by the screen tilt about an X-parallel axis through `origin`."""
    ox, oy, oz = origin
    return wp_obj.rotate((ox, oy, oz), (ox + 1, oy, oz), tilt_deg)  # top face leans toward the user (-Y)


# ---- The screen facet: a flat face sliced off the egg's top, tilted toward the user. ----
# Everything on the facet is modelled flat in "facet coordinates" (x along the egg, y in-plane,
# z = out of the facet, 0 = facet surface) and placed with on_facet().
apex_z = z_center + full_h / 2
facet_z = apex_z - facet_drop          # where the facet plane crosses the egg's centre line
lift = facet_gap + wall                # facet surface to the dongle's top face


def on_facet(wp):
    return wp.translate((0, 0, facet_z)).rotate((0, 0, facet_z), (1, 0, facet_z), tilt_deg).translate((0, face_y, 0))


def halfspace_below(z_local):
    return on_facet(cq.Workplane("XY").box(400, 400, 400, centered=(True, True, False)).translate((0, 0, z_local - 400)))


# The dongle body lies along x, back end at -x; its plug leaves through the narrow +x end.
body_len = dongle_len - plug_len
x_back = -body_len / 2 - assembly_shift
x_shoulder = x_back + body_len          # where the plug starts
x_plug_tip = x_back + dongle_len
dongle_mid_z = -lift - dongle_t / 2    # facet coordinates
dongle_env = on_facet(cq.Workplane("XY").box(body_len + clear * 2, dongle_w + clear * 2, dongle_t + clear * 2)
                      .translate((x_back + body_len / 2, 0, dongle_mid_z)))
plug_env = on_facet(cq.Workplane("XY").box(plug_len + 20, plug_w + 2 * clear, plug_h + 1.5)
                    .translate((x_shoulder + (plug_len + 20) / 2 - 0.5, 0, dongle_mid_z)))
socket_env = plug_env
lcd_x = x_back + lcd_from_back
btn_x = x_back + button_from_back

# ============================================================
# MODEL
# ============================================================
outer = egg_solid(egg_len, egg_depth, full_h, egg_skew).translate((0, 0, z_center))
inner = egg_solid(egg_len - 2 * wall, egg_depth - 2 * wall, full_h - 2 * wall, egg_skew).translate((0, 0, z_center))
outer_cut = outer.intersect(halfspace_below(0))
# The revolve leaves a hairline void along its axis; fill it so ribs come out solid.
axis_rod = cq.Workplane("YZ").circle(2.0).extrude(egg_len).translate((-egg_len / 2, 0, z_center)).intersect(inner)
inner_cut = inner.union(axis_rod).intersect(halfspace_below(-wall))
bed = cq.Workplane("XY").box(400, 400, 400, centered=(True, True, False)).translate((0, 0, -400))
shell = outer_cut.cut(inner_cut).cut(bed)
floor = cq.Workplane("XY").box(400, 400, wall, centered=(True, True, False))
shell = shell.union(outer_cut.intersect(floor))

# LCD window (with a 45-degree chamfer so the recessed screen is easy to see) and bezel recess.
window = on_facet(cq.Workplane("XY").box(lcd_view_w, lcd_view_h, 30).translate((lcd_x, 0, 0)))
chamfer = on_facet(cq.Workplane("XY").rect(lcd_view_w, lcd_view_h).workplane(offset=wall)
                   .rect(lcd_view_w + 2 * wall, lcd_view_h + 2 * wall).loft().translate((lcd_x, 0, -wall)))
bezel_w, bezel_h = lcd_view_w + 2 * bezel_lip, lcd_view_h + 2 * bezel_lip
recess = on_facet(cq.Workplane("XY").box(bezel_w + 0.3, bezel_h + 0.3, 10).edges("|Z").fillet(bezel_r + 0.15)
                  .translate((lcd_x, 0, 5 - bezel_recess)))
button_hole = on_facet(cq.Workplane("XY").circle(button_d / 2 + 0.25).extrude(30, both=True).translate((btn_x, 0, 0)))
# (No button: everything happens on the phone. Window/chamfer/recess are cut into the lid below.)

# Plug notch: open to the top so nothing has to bridge; the lid's tab closes it above the plug.
plug_top = dongle_mid_z + plug_h / 2 + clear
notch = on_facet(cq.Workplane("XY").box(30, plug_w + 2 * clear, 30, centered=(False, True, False))
                 .translate((x_shoulder - 0.5, 0, dongle_mid_z - plug_h / 2 - 0.75)))
shell = shell.cut(notch)
cable_axis = on_facet(cq.Workplane("XY").box(0.1, 0.1, 0.1).translate((x_plug_tip, 0, dongle_mid_z))).val().Center()

# Base stays flat: use 4 stick-on rubber bumpers (8 mm).

# Cradle ribs: they rise from the floor and stop at the dongle's mid-plane, so the board lies in open U-seats.
ribs = None
for x in (x_back + 6, x_back + body_len * 0.55):
    r = cq.Workplane("XY").box(rib_t, egg_depth, full_h, centered=(True, True, False)).translate((x, 0, 0))
    ribs = r if ribs is None else ribs.union(r)
# A back stop so pushing the cable on can't shove the board inward.
# It stands on the floor (a floating block would need support); its top stops at the board's top.
stop_x = on_facet(cq.Workplane("XY").box(0.1, 0.1, 0.1).translate((x_back - clear - rib_t / 2, 0, dongle_mid_z))).val().Center().x
ribs = ribs.union(cq.Workplane("XY").box(rib_t, dongle_w * 0.6, full_h, centered=(True, True, False)).translate((stop_x, 0, 0))
                  .intersect(halfspace_below(dongle_mid_z + dongle_t * 0.4)))
ribs = ribs.intersect(inner_cut).intersect(halfspace_below(dongle_mid_z + dongle_t * 0.4)).cut(dongle_env).cut(plug_env)

# Bowl: the whole shell, open at the screen face; its wall runs up to the face so no edge is knife-thin.
inner_open = inner.union(axis_rod)
bowl = outer_cut.cut(inner_open).cut(bed).union(outer_cut.intersect(floor)).union(ribs).cut(notch)
# Lid: a plate that drops into the bowl's opening (face flush with the rim), printed face-down.
plate_shape = egg_solid(egg_len - 2 * (wall + lid_fit), egg_depth - 2 * (wall + lid_fit), full_h - 2 * (wall + lid_fit),
                        egg_skew).translate((0, 0, z_center))
plate = plate_shape.intersect(outer_cut).cut(halfspace_below(-wall))
# Locating lip under the plate: a ring that slides into the bowl, kept clear of the board.
lip_slab = halfspace_below(-wall + 0.01).cut(halfspace_below(-wall - lip_h))
lip_in = egg_solid(egg_len - 2 * (wall + lid_fit + lip_t), egg_depth - 2 * (wall + lid_fit + lip_t),
                   full_h - 2 * (wall + lid_fit + lip_t), egg_skew).translate((0, 0, z_center))
board_zone = on_facet(cq.Workplane("XY").box(dongle_len + 8, dongle_w + 3, 40).translate((x_back + dongle_len / 2, 0, -10)))
lip = plate_shape.cut(lip_in).intersect(lip_slab).cut(board_zone)
# Only the long sides: at the ends the ring would hang outside the plate's outline.
lip = lip.intersect(cq.Workplane("XY").box(body_len * 0.8, 200, 200).translate((x_back + body_len / 2, 0, 0)))
# Tab that fills the plug notch in the wall above the plug.
# (it also reaches inward over the plug, joining the plate and holding the plug down)
tab = notch.intersect(outer_cut).cut(halfspace_below(plug_top + 0.2))
lid = plate.union(lip).union(tab).cut(window).cut(recess)
# Black bezel inlay: fills the recess exactly, flush with the face (printed with the lid on the AMS).
bezel = on_facet(cq.Workplane("XY").box(bezel_w + 0.3, bezel_h + 0.3, bezel_recess, centered=(True, True, False))
                 .edges("|Z").fillet(bezel_r + 0.15).translate((lcd_x, 0, -bezel_recess))).cut(window)
bottom, top, split_z = bowl, lid, 0.0

# Button (black): domed cap, a flange that keeps it from falling out, and a short stem onto the BOOT button.
cap_h = button_proud + wall
button = (cq.Workplane("XY").circle(button_d / 2).extrude(cap_h).faces(">Z").edges().fillet(1.4)
          .union(cq.Workplane("XY").circle(button_d / 2 + 1.6).extrude(-button_flange))
          .union(cq.Workplane("XY").circle(2.0).extrude(-(facet_gap - 0.3))))

# Fit test: the top band of the bowl (seats, back stop, plug notch) - prints in minutes.
fit_test = bowl.intersect(halfspace_below(-wall).cut(halfspace_below(dongle_mid_z - dongle_t)))

# Report anything poking out of the shell.
for nm, env in (("dongle", dongle_env),):
    poke = env.cut(inner_cut).val().Volume()
    bbp = env.cut(inner_cut).val().BoundingBox() if poke > 1 else None
    print(f"{nm} outside the cavity: {poke:.1f} mm3" + (f"  <-- PROBLEM at x {bbp.xmin:.0f}..{bbp.xmax:.0f} y {bbp.ymin:.0f}..{bbp.ymax:.0f} z {bbp.zmin:.0f}..{bbp.zmax:.0f}" if poke > 1 else ""))

# ============================================================
# EXPORT
# ============================================================
OUT.mkdir(exist_ok=True)
for name, part in (("egg_bowl", bowl), ("egg_lid", lid), ("bezel", bezel), ("fit_test", fit_test)):
    cq.exporters.export(part, str(OUT / f"{name}.stl"), tolerance=0.02, angularTolerance=0.05)
    bb = part.val().BoundingBox()
    print(f"{name}: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm")
# The dongle and socket envelopes, for renders and fit checks.
cq.exporters.export(dongle_env, str(OUT / "ref_dongle.stl"))
cq.exporters.export(plug_env, str(OUT / "ref_plug.stl"))

# Parts placed in their assembled positions, for renders.
bezel_placed = bezel
lcd_plane = on_facet(cq.Workplane("XY").box(lcd_view_w + 3, lcd_view_h + 3, 0.4).translate((lcd_x, 0, -lift - 0.2)))
for name, part in (("place_bezel", bezel_placed), ("place_lcd", lcd_plane)):
    cq.exporters.export(part, str(OUT / f"{name}.stl"), tolerance=0.02, angularTolerance=0.05)
import json
lc = lcd_plane.val().Center()
(OUT / "layout.json").write_text(json.dumps({
    "lcd": [lc.x, lc.y, lc.z + 0.2], "tilt_deg": tilt_deg, "lcd_w": lcd_view_w + 2, "lcd_h": lcd_view_h + 2,
    "cable_exit": [cable_axis.x, cable_axis.y, cable_axis.z], "egg_len": egg_len, "egg_h": egg_height,
    "split_z": split_z, "dongle_center": list(dongle_env.val().Center().toTuple()),
    "plug": {"w": plug_w, "h": plug_h, "len": plug_len, "tip": [cable_axis.x, cable_axis.y, cable_axis.z]},
}))
print("placed parts written")
