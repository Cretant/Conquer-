"""Builds the placeholder longsword and exports Art/Weapons/SM_Sword.fbx.

Run: blender -b -t 8 --python Tools/make_sword.py
The origin is the middle of the grip and the blade points along +X, with the edges along Y,
so the character code can put it in a fist with one offset (see DefaultGame.ini).
"""
import os

import bpy
import bmesh

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "Art", "Weapons", "SM_Sword.fbx")

bpy.ops.wm.read_factory_settings(use_empty=True)
mats = {}
for name, color, metal, rough in (("Steel", (0.62, 0.64, 0.66, 1), 1.0, 0.3),
                                  ("Leather", (0.16, 0.08, 0.04, 1), 0.0, 0.7),
                                  ("Brass", (0.55, 0.42, 0.18, 1), 1.0, 0.4)):
    m = bpy.data.materials.new(name)
    m.diffuse_color = color
    m.metallic = metal
    m.roughness = rough
    mats[name] = m

bm = bmesh.new()
parts = []


def box(x0, x1, y_half0, y_half1, z_half, mat, tip=False):
    """A box along X whose half-width tapers from y_half0 to y_half1; tip=True ends in a point."""
    s = len(bm.verts)
    for x, yh in ((x0, y_half0), (x1, y_half1)):
        zh = 0.0005 if (tip and x == x1) else z_half
        yh = 0.0005 if (tip and x == x1) else yh
        for y, z in ((yh, zh), (-yh, zh), (-yh, -zh), (yh, -zh)):
            bm.verts.new((x, y, z))
    bm.verts.ensure_lookup_table()
    v = bm.verts[s:s + 8]
    for f in ((0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
        face = bm.faces.new([v[i] for i in f])
        face.material_index = ["Steel", "Leather", "Brass"].index(mat)


# Metres: 16 cm grip centred on the origin, guard, 75 cm blade with a 12 cm point, pommel.
box(-0.08, 0.08, 0.014, 0.014, 0.012, "Leather")
box(0.08, 0.10, 0.10, 0.10, 0.012, "Brass")
box(0.10, 0.73, 0.024, 0.020, 0.004, "Steel")
box(0.73, 0.85, 0.020, 0.020, 0.004, "Steel", tip=True)
box(-0.115, -0.08, 0.022, 0.022, 0.020, "Brass")

me = bpy.data.meshes.new("SM_Sword")
bm.to_mesh(me)
for m in ("Steel", "Leather", "Brass"):
    me.materials.append(mats[m])
ob = bpy.data.objects.new("SM_Sword", me)
bpy.context.scene.collection.objects.link(ob)
ob.select_set(True)
bpy.context.view_layer.objects.active = ob
for f in me.polygons:
    f.use_smooth = False

bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True, bake_space_transform=True,
                         axis_forward="X", axis_up="Z", mesh_smooth_type="FACE", add_leaf_bones=False)
print("SWORD exported", OUT)
