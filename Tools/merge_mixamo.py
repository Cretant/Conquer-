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

rig.animation_data_create()
track = rig.animation_data.nla_tracks
for action in bpy.data.actions:
    t = track.new()
    t.name = action.name
    t.strips.new(action.name, int(action.frame_range[0]), action)

bpy.ops.export_scene.gltf(filepath=os.path.join(SRC, "SwordShield.glb"), export_format="GLB",
                          export_animation_mode="NLA_TRACKS", export_force_sampling=True)
print("MIXAMO exported", len(bpy.data.actions), "clips")
