"""Okame desk egg: a two-part egg shell around the T-Dongle-S3 clone and the USB-A socket of its cable.

Print orientation:
  egg_bottom - flat base on the bed.
  egg_top    - split face on the bed (dome up); the window opening keeps the apex short.
  bezel      - flat face on the bed.            (black)
  button     - cap face on the bed.             (black)
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
dongle_len = 58.0        # mm - whole dongle including the USB-A plug
dongle_w = 21.0          # mm
dongle_t = 9.5           # mm
plug_len = 12.0          # mm - USB-A plug length that disappears into the socket
lcd_view_w = 22.0        # mm - visible LCD area (0.96" 160x80 panel is ~21.7 x 10.8 mm)
lcd_view_h = 11.2        # mm
lcd_from_back = 22.0     # mm - LCD centre measured from the dongle's back end (non-plug end)
button_from_back = 40.0  # mm - BOOT button centre from the back end
button_on_top = True     # button on the screen side (False: on the back/underside)

# --- Cable: USB-C plug on one end, female USB-A socket overmold on the other. ESTIMATES. ---
socket_w = 18.0          # mm
socket_h = 9.0           # mm
socket_len = 30.0        # mm - overmold length including the metal socket
cable_d = 4.0            # mm

# --- Egg ---
egg_len = 104.0          # mm - along X
egg_depth = 64.0         # mm - along Y
egg_height = 50.0        # mm - after the flat base is cut
base_cut = 0.30          # fraction of the full egg height cut away to make the flat base
egg_skew = 0.07          # 0 = ellipse; >0 makes the -X end rounder and the +X end narrower (egg-like)
wall = 2.4               # mm
tilt_deg = 15.0          # screen tilt toward the user
facet_drop = 16.0        # mm - how far below the apex the screen facet is sliced
assembly_shift = 1.0     # mm - shift the dongle+socket toward the round (-X) end
facet_gap = 1.6          # mm - facet underside to the dongle's top face (room for the button flange)
split_below_mid = 0.0    # mm - split plane relative to the dongle's mid-plane (vertical)
rib_t = 2.4              # mm

# --- Fit & hardware ---
clear = 0.4              # mm - clearance around the dongle and socket (PLA on a Bambu)
split_z_frac = 0.55      # where the shell splits, as a fraction of egg_height
screw_d = 1.8            # mm - M2 self-tapping pilot
screw_head_d = 4.4       # mm
foot_d = 8.5             # mm - rubber bumper recess
foot_depth = 1.2         # mm
bezel_lip = 3.0          # mm - bezel border around the window
bezel_t = 1.6            # mm
bezel_recess = 1.0       # mm - bezel sits 0.6 mm proud
bezel_r = 3.0            # mm
button_d = 11.0          # mm - big button cap
button_proud = 1.6       # mm
button_flange = 1.0      # mm
boss_pts = [(-12.0, 19.0), (14.0, -19.0)]
boss_r = 3.4             # mm
screw_clear_d = 2.4      # mm
screw_head_h = 2.2       # mm
screw_len = 8.0          # mm - M2 x 10 total

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
    return wp.translate((0, 0, facet_z)).rotate((0, 0, facet_z), (1, 0, facet_z), tilt_deg)


def halfspace_below(z_local):
    return on_facet(cq.Workplane("XY").box(400, 400, 400, centered=(True, True, False)).translate((0, 0, z_local - 400)))


# Dongle and socket, end to end along x: back end at -x, plug into the socket toward +x.
assembly_len = dongle_len + socket_len - plug_len
x_back = -assembly_len / 2 - assembly_shift
x_plug_tip = x_back + dongle_len
x_socket_end = x_plug_tip - plug_len + socket_len
dongle_mid_z = -lift - dongle_t / 2    # facet coordinates
dongle_env = on_facet(cq.Workplane("XY").box(dongle_len + clear * 2, dongle_w + clear * 2, dongle_t + clear * 2)
                      .translate((x_back + dongle_len / 2, 0, dongle_mid_z)))
socket_cx = x_plug_tip - plug_len + socket_len / 2
socket_env = on_facet(cq.Workplane("XY").box(socket_len + clear * 2, socket_w + clear * 2, socket_h + clear * 2)
                      .translate((socket_cx, 0, dongle_mid_z)))
lcd_x = x_back + lcd_from_back
btn_x = x_back + button_from_back

# ============================================================
# MODEL
# ============================================================
outer = egg_solid(egg_len, egg_depth, full_h, egg_skew).translate((0, 0, z_center))
inner = egg_solid(egg_len - 2 * wall, egg_depth - 2 * wall, full_h - 2 * wall, egg_skew).translate((0, 0, z_center))
outer_cut = outer.intersect(halfspace_below(0))
inner_cut = inner.intersect(halfspace_below(-wall))
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
shell = shell.cut(window).cut(chamfer).cut(recess).cut(button_hole)

# Cable exit through the narrow end, with a slot down to the split so the cable drops in.
split_z = facet_z - lift - dongle_t * 0.5 - split_below_mid
cable_axis = on_facet(cq.Workplane("XY").box(0.1, 0.1, 0.1).translate((x_socket_end, 0, dongle_mid_z))).val().Center()
cable_hole = cq.Workplane("YZ").circle(cable_d / 2 + 0.5).extrude(40).translate((x_socket_end - 10, cable_axis.y, cable_axis.z))
cable_slot = cq.Workplane("XY").box(40, cable_d + 1.0, cable_axis.z - split_z + 0.1, centered=(False, True, False)) \
    .translate((x_socket_end - 10, cable_axis.y, split_z - 0.05))
shell = shell.cut(cable_hole).cut(cable_slot)

# Rubber foot recesses.
for fx, fy in ((-egg_len * 0.27, -egg_depth * 0.2), (-egg_len * 0.27, egg_depth * 0.2),
               (egg_len * 0.25, -egg_depth * 0.15), (egg_len * 0.25, egg_depth * 0.15)):
    shell = shell.cut(cq.Workplane("XY").circle(foot_d / 2).extrude(foot_depth).translate((fx, fy, 0)))

# Cradle ribs: they rise from the floor and stop at the dongle's mid-plane, so the parts lie in open U-seats.
ribs = None
for x in (x_back + 8, x_back + dongle_len * 0.55, x_plug_tip + 3, socket_cx + socket_len * 0.3):
    r = cq.Workplane("XY").box(rib_t, egg_depth, full_h, centered=(True, True, False)).translate((x, 0, 0))
    ribs = r if ribs is None else ribs.union(r)
ribs = ribs.intersect(inner_cut).intersect(halfspace_below(dongle_mid_z)).cut(dongle_env).cut(socket_env)
# Zip-tie slot under the socket for strain relief.
ribs = ribs.cut(on_facet(cq.Workplane("XY").box(rib_t + 2, socket_w + 10, 2.2)
                         .translate((socket_cx + socket_len * 0.3, 0, dongle_mid_z - socket_h / 2 - 2.5))))

# Split the shell; ribs belong to the bottom.
below = cq.Workplane("XY").box(400, 400, split_z, centered=(True, True, False))
bottom = shell.intersect(below).union(ribs)
top = shell.cut(below)

# Two M2 screws from underneath: clearance + head counterbore in the base, pilot boss in the top.
for bx, by in boss_pts:
    col = cq.Workplane("XY").circle(boss_r).extrude(full_h).translate((bx, by, 0)).intersect(inner_cut) \
        .cut(dongle_env).cut(socket_env)
    bottom = bottom.union(col.intersect(below))
    top = top.union(col.cut(below))
    bottom = bottom.cut(cq.Workplane("XY").circle(screw_clear_d / 2).extrude(split_z + 1).translate((bx, by, 0)))
    bottom = bottom.cut(cq.Workplane("XY").circle(screw_head_d / 2).extrude(screw_head_h).translate((bx, by, 0)))
    top = top.cut(cq.Workplane("XY").circle(screw_d / 2).extrude(screw_len).translate((bx, by, split_z - 0.1)))

# Bezel (black), modelled flat: prints face down.
bezel = (cq.Workplane("XY").box(bezel_w, bezel_h, bezel_t, centered=(True, True, False))
         .edges("|Z").fillet(bezel_r).faces(">Z").edges().fillet(0.6)
         .cut(cq.Workplane("XY").box(lcd_view_w, lcd_view_h, 10)))
# Button (black): domed cap, a flange that keeps it from falling out, and a short stem onto the BOOT button.
cap_h = button_proud + wall
button = (cq.Workplane("XY").circle(button_d / 2).extrude(cap_h).faces(">Z").edges().fillet(1.4)
          .union(cq.Workplane("XY").circle(button_d / 2 + 1.6).extrude(-button_flange))
          .union(cq.Workplane("XY").circle(2.0).extrude(-(facet_gap - 0.3))))

# Fit test: the bottom up to just above the seats, and the facet slice with window + button hole.
fit_test = bottom.intersect(cq.Workplane("XY").box(400, 400, split_z, centered=(True, True, False)))

# Report anything poking out of the shell.
for nm, env in (("dongle", dongle_env), ("socket", socket_env)):
    poke = env.cut(inner_cut).val().Volume()
    bbp = env.cut(inner_cut).val().BoundingBox() if poke > 1 else None
    print(f"{nm} outside the cavity: {poke:.1f} mm3" + (f"  <-- PROBLEM at x {bbp.xmin:.0f}..{bbp.xmax:.0f} y {bbp.ymin:.0f}..{bbp.ymax:.0f} z {bbp.zmin:.0f}..{bbp.zmax:.0f}" if poke > 1 else ""))

# ============================================================
# EXPORT
# ============================================================
OUT.mkdir(exist_ok=True)
for name, part in (("egg_bottom", bottom), ("egg_top", top), ("bezel", bezel), ("button", button), ("fit_test", fit_test)):
    cq.exporters.export(part, str(OUT / f"{name}.stl"), tolerance=0.02, angularTolerance=0.05)
    bb = part.val().BoundingBox()
    print(f"{name}: {bb.xlen:.1f} x {bb.ylen:.1f} x {bb.zlen:.1f} mm")
# The dongle and socket envelopes, for renders and fit checks.
cq.exporters.export(dongle_env, str(OUT / "ref_dongle.stl"))
cq.exporters.export(socket_env, str(OUT / "ref_socket.stl"))

# Parts placed in their assembled positions, for renders.
bezel_placed = on_facet(bezel.translate((lcd_x, 0, -bezel_recess)))
button_placed = on_facet(button.translate((btn_x, 0, -wall)))
lcd_plane = on_facet(cq.Workplane("XY").box(lcd_view_w + 3, lcd_view_h + 3, 0.4).translate((lcd_x, 0, -lift - 0.2)))
for name, part in (("place_bezel", bezel_placed), ("place_button", button_placed), ("place_lcd", lcd_plane)):
    cq.exporters.export(part, str(OUT / f"{name}.stl"), tolerance=0.02, angularTolerance=0.05)
import json
lc = lcd_plane.val().Center()
(OUT / "layout.json").write_text(json.dumps({
    "lcd": [lc.x, lc.y, lc.z + 0.2], "tilt_deg": tilt_deg, "lcd_w": lcd_view_w + 2, "lcd_h": lcd_view_h + 2,
    "cable_exit": [x_socket_end + 9, cable_axis.y, cable_axis.z], "egg_len": egg_len, "egg_h": egg_height,
    "split_z": split_z, "dongle_center": list(dongle_env.val().Center().toTuple()),
}))
print("placed parts written")
