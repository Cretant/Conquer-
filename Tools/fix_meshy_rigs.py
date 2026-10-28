"""Re-exports Meshy's rigged characters so Unreal gets a clean skeleton: Meshy's FBX puts a x100 scale on the armature
root with every bone in metres, and Unreal's retargeter drops that scale (the retargeted clips collapse the body).
Here the scale is baked into the bones (centimetres, root scale 1) and the root bone is named "root" like the
mannequin's. Writes Art/Meshy/<Name>/<Name>_rig.fbx (textures embedded), which Tools/import_meshy.py prefers.

Run: blender -b -t 8 --python Tools/fix_meshy_rigs.py
"""
import glob
import os

import bpy

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MESHY = os.path.join(REPO, "Art", "Meshy")

for name in ("Wren", "Soldier", "Garrick"):
    files = glob.glob(os.path.join(MESHY, name, "**", "*Character_output.fbx"), recursive=True)
    if not files:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=files[0], use_anim=False)
    arm = next(o for o in bpy.context.scene.objects if o.type == "ARMATURE")
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]

    # Bake the armature object's transform (Meshy's 0.01 scale) into its bones and the skinned meshes.
    bpy.ops.object.select_all(action="DESELECT")
    for o in [arm] + meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # Work in centimetres, Unreal's unit, so the FBX needs no unit scale on the armature node.
    # Unparent the meshes first, or scaling the armature scales them twice.
    bpy.context.scene.unit_settings.scale_length = 0.01
    for m in meshes:
        m.parent = None
    for o in [arm] + meshes:
        o.scale = (100.0, 100.0, 100.0)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for m in meshes:
        m.parent = arm
    arm.name = "root"
    arm.data.name = "root"
    if arm.animation_data:
        arm.animation_data_clear()

    out = os.path.join(MESHY, name, name + "_rig.fbx")
    bpy.ops.export_scene.fbx(filepath=out, use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", add_leaf_bones=False,
                             bake_anim=False, path_mode="COPY", embed_textures=True, mesh_smooth_type="FACE")
    bones = arm.data.bones
    print("RIG %s -> %s, %d bones, armature scale %s, hand_r at %s" % (
        name, out, len(bones), tuple(arm.scale), tuple(round(v, 3) for v in bones["hand_r"].head_local)))
