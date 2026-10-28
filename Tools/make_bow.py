"""Builds the archers' placeholder bow and exports Art/Weapons/SM_Bow.fbx.

Run: blender -b -t 8 --python Tools/make_bow.py
Same frame as the swords (Tools/make_sword.py): the origin is the grip and the limbs run along X, curving back
towards -Y where the string runs tip to tip, so DefaultGame.ini can reuse a sword grip offset.
"""
import os

import bmesh
import bpy

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(REPO, "Art", "Weapons", "SM_Bow.fbx")
SLOTS = ["Wood", "String"]
HALF = 0.65     # half the bow's length, metres
BEND = 0.16     # how far the tips sit behind the grip

bpy.ops.wm.read_factory_settings(use_empty=True)
mats = {}
for name, color in (("Wood", (0.25, 0.15, 0.07, 1)), ("String", (0.75, 0.72, 0.62, 1))):
    m = bpy.data.materials.new(name)
    m.diffuse_color = color
    m.roughness = 0.8
    mats[name] = m

bm = bmesh.new()


def beam(a, b, half_w, half_h, mat):
    """A box from point a to point b in the XY plane."""
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    length = (dx * dx + dy * dy) ** 0.5
    nx, ny = -dy / length * half_w, dx / length * half_w
    s = len(bm.verts)
    for px, py in ((ax, ay), (bx, by)):
        for ox, oy, z in ((nx, ny, half_h), (-nx, -ny, half_h), (-nx, -ny, -half_h), (nx, ny, -half_h)):
            bm.verts.new((px + ox, py + oy, z))
    bm.verts.ensure_lookup_table()
    v = bm.verts[s:s + 8]
    for f in ((0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)):
        face = bm.faces.new([v[i] for i in f])
        face.material_index = SLOTS.index(mat)


# The limbs: a curve of short beams, thicker at the grip.
steps = 12
points = []
for i in range(steps + 1):
    x = -HALF + 2 * HALF * i / steps
    points.append((x, -BEND * (x / HALF) ** 2))
for i in range(steps):
    mid = abs((points[i][0] + points[i + 1][0]) / 2) / HALF
    beam(points[i], points[i + 1], 0.018 - 0.008 * mid, 0.014 - 0.006 * mid, "Wood")
beam(points[0], points[-1], 0.003, 0.003, "String")

me = bpy.data.meshes.new("SM_Bow")
bm.to_mesh(me)
for m in SLOTS:
    me.materials.append(mats[m])
ob = bpy.data.objects.new("SM_Bow", me)
bpy.context.scene.collection.objects.link(ob)
ob.select_set(True)
bpy.context.view_layer.objects.active = ob

bpy.ops.export_scene.fbx(filepath=OUT, use_selection=True, apply_unit_scale=True, bake_space_transform=True,
                         axis_forward="X", axis_up="Z", mesh_smooth_type="FACE", add_leaf_bones=False)
print("BOW exported", OUT)
