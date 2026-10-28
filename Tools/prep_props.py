"""Prepares the Meshy sword and shield for the characters' hands, writing Art/Weapons/SM_MeshySword.fbx and
SM_MeshyShield.fbx (textures embedded).

- Sword: scaled to 95 cm long, blade along +X, edges along Y, origin in the middle of the grip.
- Shield: scaled to 60 cm across, face pointing +X, up +Z, origin at the middle of its back (where the arm goes).

Run: blender -b -t 8 --python Tools/prep_props.py
"""
import glob
import os

import bpy
import numpy as np
from mathutils import Matrix, Vector

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MESHY = os.path.join(REPO, "Art", "Meshy")
OUT = os.path.join(REPO, "Art", "Weapons")


def load(folder, pattern="*.fbx"):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    files = sorted(glob.glob(os.path.join(MESHY, folder, "**", pattern), recursive=True))
    bpy.ops.import_scene.fbx(filepath=files[-1])
    ob = next(o for o in bpy.context.scene.objects if o.type == "MESH")
    # Bake the import transform into the mesh so vertex coordinates are world metres.
    ob.data.transform(ob.matrix_world)
    ob.matrix_world = Matrix.Identity(4)
    for o in list(bpy.context.scene.objects):
        if o is not ob:
            bpy.data.objects.remove(o, do_unlink=True)
    return ob, np.array([v.co[:] for v in ob.data.vertices])


def apply(ob, basis, origin, scale):
    """basis rows: new X, Y, Z axes in old space; origin: old-space point that becomes (0, 0, 0)."""
    rot = Matrix([list(basis[0]) + [0], list(basis[1]) + [0], list(basis[2]) + [0], [0, 0, 0, 1]])
    m = Matrix.Scale(scale, 4) @ rot @ Matrix.Translation(-Vector(origin))
    ob.data.transform(m)
    ob.data.update()


def export(ob, name):
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    ob.name = ob.data.name = name
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, bake_space_transform=True,
                             axis_forward="X", axis_up="Z", path_mode="COPY", embed_textures=True,
                             mesh_smooth_type="FACE", add_leaf_bones=False)
    lo, hi = np.array(ob.bound_box[0]), np.array(ob.bound_box[6])
    print("PROP %s -> %s, bounds %s .. %s m" % (name, path, np.round(lo, 3), np.round(hi, 3)))


# --- Sword: the long axis is the blade; the crossguard is the widest slice, and the grip is just past it. ---
ob, v = load("Sword2")
c = v.mean(0)
_, _, vt = np.linalg.svd(v - c, full_matrices=False)
axis, width_axis = vt[0], vt[1]
t = (v - c) @ axis
w = np.abs((v - c) @ width_axis)
bins = np.linspace(t.min(), t.max(), 41)
widths = [w[(t >= a) & (t < b)].max(initial=0) for a, b in zip(bins[:-1], bins[1:])]
guard_t = (bins[np.argmax(widths)] + bins[np.argmax(widths) + 1]) / 2
length = t.max() - t.min()
if guard_t > (t.min() + t.max()) / 2:
    axis, t, guard_t = -axis, -t, -guard_t  # point +axis from the guard towards the tip
scale = 0.95 / length
grip_t = guard_t - 0.07 / scale  # the middle of the grip, 7 cm (final size) behind the guard
origin = c + axis * grip_t
apply(ob, np.array([axis, width_axis, np.cross(axis, width_axis)]), origin, scale)
export(ob, "SM_MeshySword")

# --- Shield: the thinnest axis is the face normal; the front is the side the boss and crest stick out of. ---
ob, v = load("Shield")
c = v.mean(0)
_, _, vt = np.linalg.svd(v - c, full_matrices=False)
normal = vt[2]
depth = (v - c) @ normal
if abs(depth.max()) < abs(depth.min()):
    normal, depth = -normal, -depth
up = np.array([0.0, 0.0, 1.0]) - normal * normal[2]
up /= np.linalg.norm(up)
diameter = max(np.ptp((v - c) @ vt[0]), np.ptp((v - c) @ vt[1]))
scale = 0.60 / diameter
origin = c + normal * depth.min()  # the back face, where the forearm sits
apply(ob, np.array([normal, np.cross(up, normal), up]), origin, scale)
export(ob, "SM_MeshyShield")
