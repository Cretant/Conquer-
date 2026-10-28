"""Builds the placeholder pitchfork Wren starts Marrowford with and exports Art/Weapons/SM_Pitchfork.fbx.

Run: blender -b -t 8 --python Tools/make_pitchfork.py
Same frame as the swords (Tools/make_sword.py): the origin is where her hand grips the shaft and the tines point
along +X, spread along Y, so DefaultGame.ini can reuse the sword's grip offset.
"""
import os

import bpy
import bmesh

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "Art", "Weapons", "SM_Pitchfork.fbx")
SLOTS = ["Wood", "Iron"]

bpy.ops.wm.read_factory_settings(use_empty=True)
mats = {}
for name, color, metal, rough in (("Wood", (0.30, 0.19, 0.10, 1), 0.0, 0.8),
                                  ("Iron", (0.22, 0.22, 0.23, 1), 1.0, 0.55)):
    m = bpy.data.materials.new(name)
    m.diffuse_color = color
    m.metallic = metal
    m.roughness = rough
    mats[name] = m

bm = bmesh.new()


def box(x0, x1, y0, y1, z_half, mat, tip=False):
    """A box along X spanning y0..y1; tip=True narrows the far end to a point."""
    s = len(bm.verts)
    yc = (y0 + y1) / 2
    for x in (x0, x1):
        pinch = tip and x == x1
        ya, yb = (yc - 0.0005, yc + 0.0005) if pinch else (y0, y1)
        zh = 0.0005 if pinch else z_half
        for y, z in ((yb, zh), (ya, zh), (ya, -zh), (yb, -zh)):
            bm.verts.new((x, y, z))
    bm.verts.ensure_lookup_table()
    v = bm.verts[s:s + 8]
    for f in ((0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
        face = bm.faces.new([v[i] for i in f])
        face.material_index = SLOTS.index(mat)


# Metres: a 1.5 m ash shaft held a third of the way up, an iron neck and crossbar, three 27 cm tines.
box(-0.55, 0.95, -0.016, 0.016, 0.016, "Wood")
box(0.95, 1.02, -0.022, 0.022, 0.022, "Iron")
box(1.02, 1.05, -0.090, 0.090, 0.012, "Iron")
for y in (-0.078, 0.0, 0.078):
    box(1.05, 1.32, y - 0.006, y + 0.006, 0.006, "Iron", tip=True)

me = bpy.data.meshes.new("SM_Pitchfork")
bm.to_mesh(me)
for m in SLOTS:
    me.materials.append(mats[m])
ob = bpy.data.objects.new("SM_Pitchfork", me)
bpy.context.scene.collection.objects.link(ob)
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
for f in me.polygons:
    f.use_smooth = False

bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True, bake_space_transform=True,
                         axis_forward="X", axis_up="Z", mesh_smooth_type="FACE", add_leaf_bones=False)
print("PITCHFORK exported", OUT)
