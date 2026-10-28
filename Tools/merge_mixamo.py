"""Merges Mixamo's Sword And Shield Pack (Art/Mixamo, one FBX per clip) into Art/Mixamo/SwordShield.glb:
the X Bot mesh with every clip as a named animation, ready for Tools/build_sword.py.

Run: blender -b -t 8 --python Tools/merge_mixamo.py
"""
import glob
import os

import bpy

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "Art", "Mixamo")

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=os.path.join(SRC, "X Bot.fbx"))
rig = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
for action in list(bpy.data.actions):
    bpy.data.actions.remove(action)

for path in sorted(glob.glob(os.path.join(SRC, "*.fbx"))):
    name = os.path.splitext(os.path.basename(path))[0]
    if name == "X Bot":
        continue
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=path)
    added = set(bpy.context.scene.objects) - before
    clip = next(o for o in added if o.type == "ARMATURE").animation_data.action
    # e.g. "sword and shield slash (2)" -> "Slash_2", "sheath sword 1" -> "Sheath_Sword_1"
    label = name.replace("sword and shield ", "").replace("(", "").replace(")", "").title().replace(" ", "_")
    clip.name = label
    clip.use_fake_user = True
    for ob in added:
        bpy.data.objects.remove(ob, do_unlink=True)
    print("MIXAMO %s %d-%d" % (label, clip.frame_range[0], clip.frame_range[1]))

# Bake the armature's 0.01 import scale into its bones and mesh, and scale every location key to match: left on the
# armature node, Unreal's retargeter reads the hip motion 100x too large and the clips' root ends up metres away.
scale = rig.scale[0]
meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
for m in meshes:
    m.parent = None
bpy.ops.object.select_all(action="DESELECT")
for o in [rig] + meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
for m in meshes:
    m.parent = rig
def curves_of(action):
    if hasattr(action, "fcurves"):
        return list(action.fcurves)
    # Blender's layered actions keep their curves in channel bags.
    return [c for layer in action.layers for strip in layer.strips for bag in strip.channelbags for c in bag.fcurves]


for action in bpy.data.actions:
    for curve in curves_of(action):
        if curve.data_path.endswith(".location"):
            for key in curve.keyframe_points:
                key.co[1] *= scale
                key.handle_left[1] *= scale
                key.handle_right[1] *= scale

rig.animation_data_create()
track = rig.animation_data.nla_tracks
for action in bpy.data.actions:
    t = track.new()
    t.name = action.name
    t.strips.new(action.name, int(action.frame_range[0]), action)

bpy.ops.export_scene.gltf(filepath=os.path.join(SRC, "SwordShield.glb"), export_format="GLB",
                          export_animation_mode="NLA_TRACKS", export_force_sampling=True)
print("MIXAMO exported", len(bpy.data.actions), "clips")
