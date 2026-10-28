"""Builds the sword content: sword clips retargeted onto the UE5 mannequin from Quaternius' Universal Animation
Library 1 and 2 (CC0) and, when present, Mixamo's Sword And Shield Pack; plus the placeholder sword mesh
(Art/Weapons/SM_Sword.fbx, made by Tools/make_sword.py).

Needs the raw packs under Art/ (see Art/LICENSES.md; Mixamo merged first by Tools/merge_mixamo.py). Run from the
repo root:
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_sword.py" -unattended -nosplash

Retargeted clips land in /Game/Conquer/Animations/Sword as A_<clip>, e.g. A_Sword_Regular_A; Mixamo's go to
/Game/Conquer/Animations/Mixamo/Clips (A_MX_<clip>), which stays out of git.
Tools/build_anims.py makes the montages.
"""
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(REPO, "Art")
OUT = "/Game/Conquer/Animations/Sword"
# Mixamo's clips may not be redistributed, so they go to a folder that stays out of git.
OUT_MIXAMO = "/Game/Conquer/Animations/Mixamo/Clips"
RIGS = "/Game/Conquer/Animations/Retarget"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"

# Pack -> (file under Art/, clip-name prefix on the retargeted clips, clips to retarget)
PACKS = {
    "UAL1": ("Quaternius/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb", "",
             ["Sword_Attack", "Sword_Idle", "Hit_Chest", "Hit_Head", "Death01", "Roll", "Fixing_Kneeling",
              # Pitchfork thrusts (jabs), the archers' aim and loose, and the cutscene soldiers' torches.
              "Punch_Jab", "Punch_Cross", "Pistol_Aim_Neutral", "Pistol_Shoot", "Idle_Torch_Loop"]),
    "UAL2": ("Quaternius/Universal Animation Library 2[Standard]/Unreal-Godot/UAL2_Standard.glb", "",
             ["Sword_Regular_A", "Sword_Regular_B", "Sword_Regular_C", "Sword_Heavy_Combo", "Sword_Block",
              "Sword_Dash", "Hit_Knockback", "Melee_Hook",
              # Getting up off the ground (the opening), and a longer combo for executions.
              "LayToIdle", "Sword_Regular_Combo"]),
    # Mixamo's Sword And Shield Pack, merged into one GLB by Tools/merge_mixamo.py. Optional: skipped if absent.
    "Mixamo": ("Mixamo/SwordShield.glb", "MX_",
               ["Slash", "Slash_2", "Slash_3", "Slash_4", "Slash_5", "Attack", "Attack_2", "Attack_3", "Attack_4",
                "Kick", "Block", "Block_2", "Block_Idle", "Idle",
                "Impact", "Impact_2", "Impact_3", "Death", "Death_2", "Power_Up",
                # Armed locomotion (forward/back walk and run, strafes: 1 walk right, 2 walk left, 3 run left,
                # 4 run right), measured from the hips' travel in each clip.
                "Walk", "Walk_2", "Run", "Run_2", "Strafe", "Strafe_2", "Strafe_3", "Strafe_4"]),
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log("[build_sword] " + msg)


def wipe(game_dir):
    # Delete on disk: the commandlet's asset registry may not know about the last run's packages yet.
    folder = os.path.join(REPO, "Content", game_dir[len("/Game/"):])
    shutil.rmtree(folder, ignore_errors=True)


def import_pack(name, rel):
    dest = "/Game/ThirdParty/" + name
    task = unreal.AssetImportTask()
    task.filename = os.path.join(ART, rel)
    task.destination_path = dest
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])

    found = registry.get_assets_by_path(dest, recursive=True)
    meshes = [a for a in found if a.asset_class_path.asset_name == "SkeletalMesh"]
    anims = [a for a in found if a.asset_class_path.asset_name == "AnimSequence"]
    log("%s: %d assets, %d meshes, %d animations" % (name, len(found), len(meshes), len(anims)))
    if not meshes:
        raise RuntimeError("no skeletal mesh imported from " + rel)
    return meshes[0].get_asset(), anims


def make_ik_rig(name, mesh):
    path = RIGS + "/" + name
    rig = tools.create_asset(name, RIGS, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    ctrl.apply_auto_generated_retarget_definition()
    assets.save_asset(path, only_if_is_dirty=False)
    log("%s: root %s, chains %s" % (name, ctrl.get_retarget_root(),
                                    [str(c.get_editor_property("chain_name")) for c in ctrl.get_retarget_chains()]))
    return rig



def ground_root_motion(ctrl):
    """Root motion follows the pelvis along the floor: with the default (copy the source root, height and all) the
    root rides at hip height, and once the characters lock it to the floor the body sinks into it."""
    for i in range(ctrl.get_num_retarget_ops()):
        if str(ctrl.get_op_name(i)) == "Root Motion":
            op = ctrl.get_op_controller(i)
            settings = op.get_settings()
            settings.root_motion_source = unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS
            settings.root_height_source = unreal.RootMotionHeightSource.SNAP_TO_GROUND
            op.set_settings(settings)


def make_retargeter(name, source_rig, target_rig, source_mesh, target_mesh):
    path = RIGS + "/" + name
    rtg = tools.create_asset(name, RIGS, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source_mesh)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target_mesh)
    ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    # The UAL rig rests in a T-pose and the mannequin in an A-pose: line the target's bones up with the source's.
    ctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    ground_root_motion(ctrl)
    log("%s root settings: %s" % (name, ctrl.get_root_settings()))
    assets.save_asset(path, only_if_is_dirty=False)
    return rtg


def short_name(asset_data):
    # Interchange prefixes each clip with the file name, e.g. UAL1_StandardSword_Attack.
    return str(asset_data.asset_name)


WEAPONS = "/Game/Conquer/Weapons"

# The sword is fitted to the closed fist of the clip below (the main attack source): the grip runs from the
# pinky to the index finger through the middle of the curled fingers and thumb, and the edge faces the way the
# knuckles point. The result is logged for WeaponLocation / WeaponRotation in Config/DefaultGame.ini.
GRIP_CLIPS = [OUT_MIXAMO + "/A_MX_Slash", OUT + "/A_Sword_Regular_A"]
KNUCKLES = unreal.Vector(0.0, -1.0, 0.0)


def measure_grip():
    clip = next(assets.load_asset(c) for c in GRIP_CLIPS if assets.does_asset_exist(c))
    poses = unreal.AnimPoseExtensions
    pose = poses.get_anim_pose_at_time(clip, 0.1, unreal.AnimPoseEvaluationOptions())
    hand = poses.get_bone_pose(pose, "hand_r", unreal.AnimPoseSpaces.WORLD).inverse()

    def at(bone):
        return hand.transform_location(poses.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.WORLD).translation)

    wrap = [at(b) for b in ("index_02_r", "middle_02_r", "ring_02_r", "pinky_02_r", "thumb_02_r")]
    centre = unreal.Vector(sum(v.x for v in wrap) / 5, sum(v.y for v in wrap) / 5, sum(v.z for v in wrap) / 5)
    axis = (at("index_02_r") - at("pinky_02_r")).normal()
    return clip.get_name(), centre, axis


def make_material(name, color, metallic, roughness, parent):
    mi = tools.create_asset(name, WEAPONS, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    mel = unreal.MaterialEditingLibrary
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    assets.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    return mi


def build_sword_mesh():
    mel = unreal.MaterialEditingLibrary
    base = tools.create_asset("M_Placeholder", WEAPONS, unreal.Material, unreal.MaterialFactoryNew())
    for i, (param, prop, cls) in enumerate((("Color", unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialExpressionVectorParameter),
                                            ("Metallic", unreal.MaterialProperty.MP_METALLIC, unreal.MaterialExpressionScalarParameter),
                                            ("Roughness", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialExpressionScalarParameter))):
        node = mel.create_material_expression(base, cls, -400, i * 150)
        node.set_editor_property("parameter_name", param)
        mel.connect_material_property(node, "", prop)
    mel.recompile_material(base)
    assets.save_asset(base.get_path_name(), only_if_is_dirty=False)
    slots = {"Steel": make_material("MI_Steel", (0.62, 0.64, 0.66, 1), 1.0, 0.3, base),
             "Leather": make_material("MI_Leather", (0.16, 0.08, 0.04, 1), 0.0, 0.7, base),
             "Brass": make_material("MI_Brass", (0.55, 0.42, 0.18, 1), 1.0, 0.4, base)}

    task = unreal.AssetImportTask()
    task.filename = os.path.join(REPO, "Art", "Weapons", "SM_Sword.fbx")
    task.destination_path = WEAPONS
    task.destination_name = "SM_Sword"
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(WEAPONS + "/SM_Sword")
    if not mesh:
        raise RuntimeError("sword import failed")
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        match = next((m for key, m in slots.items() if name.startswith(key)), None)
        if match:
            mesh.set_material(i, match)
    for asset_path in assets.list_assets(WEAPONS, recursive=False):
        # Interchange also imports the FBX's own materials; the instances above replace them.
        if assets.find_asset_data(asset_path).asset_class_path.asset_name == "Material" and "M_Placeholder" not in asset_path:
            assets.delete_asset(asset_path)
    assets.save_asset(WEAPONS + "/SM_Sword", only_if_is_dirty=False)

    box = mesh.get_bounding_box()
    clip, centre, axis = measure_grip()
    edge = KNUCKLES - axis * KNUCKLES.dot(axis)
    rot = unreal.MathLibrary.make_rot_from_xy(axis, edge.normal())
    log("sword bounds min %s max %s; grip measured on %s" % (box.min, box.max, clip))
    log("WeaponLocation=(X=%.2f,Y=%.2f,Z=%.2f)" % (centre.x, centre.y, centre.z))
    log("WeaponRotation=(Pitch=%.2f,Yaw=%.2f,Roll=%.2f)" % (rot.pitch, rot.yaw, rot.roll))


for folder in (RIGS, OUT, OUT_MIXAMO, WEAPONS, "/Game/ThirdParty", "/Game/Quaternius"):
    wipe(folder)

target_mesh = assets.load_asset(TARGET_MESH)
target_rig = make_ik_rig("IK_Mannequin", target_mesh)

for pack, (rel, prefix, wanted) in PACKS.items():
    if not os.path.exists(os.path.join(ART, rel)):
        log("%s: %s not found, skipped" % (pack, rel))
        continue
    source_mesh, anims = import_pack(pack, rel)
    file_prefix = os.path.splitext(os.path.basename(rel))[0]
    log("%s clips: %s" % (pack, sorted(short_name(a) for a in anims)))
    source_rig = make_ik_rig("IK_" + pack, source_mesh)
    rtg = make_retargeter("RTG_" + pack + "_To_Mannequin", source_rig, target_rig, source_mesh, target_mesh)

    picked = []
    for clip in wanted:
        match = [a for a in anims if short_name(a) in (clip, file_prefix + clip)]
        if not match:
            raise RuntimeError("%s has no clip %s" % (pack, clip))
        picked.append(match[0])

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = picked
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = rtg
    inputs.target_path = OUT_MIXAMO if pack == "Mixamo" else OUT
    inputs.search = file_prefix
    inputs.replace = prefix
    inputs.prefix = "A_"
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = False  # folders are wiped first; overwriting stale registry entries crashes
    for asset_data in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs):
        asset = asset_data.get_asset()
        if isinstance(asset, unreal.AnimSequence):
            assets.save_asset(asset.get_path_name(), only_if_is_dirty=False)
            log("retargeted %s (%.2fs)" % (asset.get_path_name(), asset.get_play_length()))

build_sword_mesh()
log("done")
