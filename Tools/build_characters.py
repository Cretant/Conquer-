"""Gives the Meshy characters (Tools/import_meshy.py) the game's animations, retargeted from the mannequin onto each
character's own skeleton:
- the locomotion clips (through the mannequin's animation blueprint) and the CC0 combat montages, into
  /Game/Characters/Conquer/<Name>/Anims (in git);
- the Mixamo montages from /Game/Conquer/Animations/Mixamo, when present, into .../<Name>/Mixamo (local only:
  Mixamo's animations must not be redistributed, and the game prefers them when they exist).

It also measures where the sword sits in the right fist and the shield on the left hand, and the mesh's facing
and feet, and logs the matching Config/DefaultGame.ini lines.

Run from the repo root after Tools/build_sword.py and Tools/build_anims.py (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_characters.py" -unattended -nosplash
"""
import math
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CHARS = "/Game/Characters/Conquer"
RIGS = CHARS + "/Retarget"
SOURCE_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
ABP = "/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"
MONTAGES = "/Game/Conquer/Animations"
MIXAMO_MONTAGES = MONTAGES + "/Mixamo"
NAMES = ["Wren", "Soldier", "Garrick"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
poses = unreal.AnimPoseExtensions
WORLD = unreal.AnimPoseSpaces.WORLD


def log(msg):
    unreal.log("[build_characters] " + msg)


def wipe(game_dir):
    shutil.rmtree(os.path.join(REPO, "Content", game_dir[len("/Game/"):]), ignore_errors=True)


def make_ik_rig(name, mesh):
    rig = tools.create_asset(name, RIGS, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    ctrl.apply_auto_generated_retarget_definition()
    assets.save_asset(rig.get_path_name(), only_if_is_dirty=False)
    return rig


def make_retargeter(name, source_rig, target_rig, source_mesh, target_mesh):
    rtg = tools.create_asset(name, RIGS, unreal.IKRetargeter, unreal.IKRetargetFactory())
    ctrl = unreal.IKRetargeterController.get_controller(rtg)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.SOURCE, source_rig)
    ctrl.set_ik_rig(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.SOURCE, source_mesh)
    ctrl.set_preview_mesh(unreal.RetargetSourceOrTarget.TARGET, target_mesh)
    ctrl.add_default_ops()
    ctrl.assign_ik_rig_to_all_ops(unreal.RetargetSourceOrTarget.TARGET, target_rig)
    ctrl.auto_map_chains(unreal.AutoMapChainType.FUZZY, True)
    ctrl.auto_align_all_bones(unreal.RetargetSourceOrTarget.TARGET)
    assets.save_asset(rtg.get_path_name(), only_if_is_dirty=False)
    return rtg


def bone_at(pose, bone):
    return poses.get_bone_pose(pose, bone, WORLD)


def rel_offset(parent, location, x_axis, z_axis):
    """Location and rotation, relative to the parent bone, of a frame at `location` with the given X and Z axes."""
    rot = unreal.MathLibrary.make_rot_from_xz(x_axis, z_axis)
    world = unreal.Transform(location, rot, unreal.Vector(1, 1, 1))
    rel = world.make_relative(parent)
    return rel.translation, rel.rotation.rotator()


def measure(name, slash, idle):
    opts = unreal.AnimPoseEvaluationOptions()
    pose = poses.get_anim_pose_at_time(slash, 0.1, opts)
    hand = bone_at(pose, "hand_r")
    inv = hand.inverse()
    at = lambda b: inv.transform_location(bone_at(pose, b).translation)
    wrap = [at(b) for b in ("index_02_r", "middle_02_r", "ring_02_r", "pinky_02_r", "thumb_02_r")]
    centre = unreal.Vector(sum(v.x for v in wrap) / 5, sum(v.y for v in wrap) / 5, sum(v.z for v in wrap) / 5)
    axis = (at("index_02_r") - at("pinky_02_r")).normal()
    knuckles = at("middle_01_r").normal()
    edge = (knuckles - axis * knuckles.dot(axis)).normal()
    rot = unreal.MathLibrary.make_rot_from_xy(axis, edge)
    weapon = "WeaponLocation=(X=%.2f,Y=%.2f,Z=%.2f)\nWeaponRotation=(Pitch=%.2f,Yaw=%.2f,Roll=%.2f)" % (
        centre.x, centre.y, centre.z, rot.pitch, rot.yaw, rot.roll)

    # The shield faces where the character faces (toes forward) and stands upright, its back on the left hand.
    pose = poses.get_anim_pose_at_time(idle, 0.5, opts)
    fwd = bone_at(pose, "ball_l").translation - bone_at(pose, "foot_l").translation
    fwd = unreal.Vector(fwd.x, fwd.y, 0).normal()
    hand_l = bone_at(pose, "hand_l")
    loc, srot = rel_offset(hand_l, hand_l.translation + fwd * 4.0, fwd, unreal.Vector(0, 0, 1))
    shield = "ShieldLocation=(X=%.2f,Y=%.2f,Z=%.2f)\nShieldRotation=(Pitch=%.2f,Yaw=%.2f,Roll=%.2f)" % (
        loc.x, loc.y, loc.z, srot.pitch, srot.yaw, srot.roll)

    # The actor faces +X; turn the mesh so its toes point that way.
    yaw = -math.degrees(math.atan2(fwd.y, fwd.x))
    return weapon, shield, yaw


def montages_in(folder):
    return [a for a in registry.get_assets_by_path(folder, recursive=False)
            if str(a.asset_class_path.asset_name) == "AnimMontage"]


def retarget(to_retarget, source_mesh, mesh, rtg, folder, suffix):
    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = to_retarget
    inputs.source_mesh = source_mesh
    inputs.target_mesh = mesh
    inputs.ik_retarget_asset = rtg
    inputs.target_path = folder
    inputs.suffix = suffix
    inputs.include_referenced_assets = True
    inputs.overwrite_existing_files = False  # the folder is wiped first; overwriting stale registry entries crashes
    made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    for a in made:
        asset = a.get_asset()
        if isinstance(asset, unreal.AnimSequence):
            # Retargeting the Mixamo clips a second time turns their hip motion into a root track metres away; with
            # root motion on (and ignored by the characters) the root stays locked to the reference pose.
            asset.set_editor_property("enable_root_motion", True)
            asset.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
        assets.save_asset(str(a.package_name), only_if_is_dirty=False)
    # The 5.8 batch retarget duplicates the animation blueprint's clips but not the blueprint itself (its duplicate
    # comes back null), which is why the characters use the native UConquerAnimInstance instead.
    return {str(a.asset_name): "%s.%s" % (a.package_name, a.asset_name) for a in made}


source_mesh = assets.load_asset(SOURCE_MESH)
wipe(RIGS)
source_rig = make_ik_rig("IK_Manny", source_mesh)
abp = registry.get_asset_by_object_path(ABP + ".ABP_Unarmed")
public_montages = montages_in(MONTAGES)
mixamo_montages = montages_in(MIXAMO_MONTAGES)

for name in NAMES:
    folder = CHARS + "/" + name
    meshes = [a for a in registry.get_assets_by_path(folder, recursive=False)
              if str(a.asset_class_path.asset_name) == "SkeletalMesh"]
    if not meshes:
        log("%s: no skeletal mesh in %s, skipped (run Tools/import_meshy.py)" % (name, folder))
        continue
    mesh = meshes[0].get_asset()
    wipe(folder + "/Anims")
    wipe(folder + "/Mixamo")

    target_rig = make_ik_rig("IK_" + name, mesh)
    rtg = make_retargeter("RTG_Manny_To_" + name, source_rig, target_rig, source_mesh, mesh)
    suffix = "_" + name
    public = retarget([abp] + public_montages, source_mesh, mesh, rtg, folder + "/Anims", suffix)
    mixamo = retarget(mixamo_montages, source_mesh, mesh, rtg, folder + "/Mixamo", suffix) if mixamo_montages else {}
    path = lambda n: public[n + suffix]

    # Fit the sword to the fist of the clips the game will actually use on this PC.
    slash = mixamo.get("A_MX_Slash" + suffix) or public["A_Sword_Regular_A" + suffix]
    weapon, shield, yaw = measure(name, assets.load_asset(slash), assets.load_asset(path("MM_Idle")))

    box = mesh.get_bounds()
    half_height = box.box_extent.z
    bottom = box.origin.z - half_height
    radius = max(30.0, min(60.0, half_height * 0.42))
    montage = lambda key, n: "%sMontagePath=%s" % (key, path(n))
    lines = [
        "BodyMeshPath=" + mesh.get_path_name(),
        "AnimClassPath=/Script/Conquer.ConquerAnimInstance",
        "IdleAnimPath=" + path("MM_Idle"),
        "WalkAnimPath=" + path("MF_Unarmed_Walk_Fwd"),
        "JogAnimPath=" + path("MF_Unarmed_Jog_Fwd"),
        "CapsuleRadius=%.1f" % radius,
        "CapsuleHalfHeight=%.1f" % half_height,
        "MeshLocation=(X=0,Y=0,Z=%.1f)" % (-half_height - bottom),
        "MeshRotation=(Pitch=0,Yaw=%.1f,Roll=0)" % yaw,
        "WeaponMeshPath=/Game/Props/Sword/SM_MeshySword.SM_MeshySword",
        weapon,
        shield,
        montage("LightLeft", "AM_Light_Left"), montage("LightTop", "AM_Light_Top"),
        montage("LightRight", "AM_Light_Right"), montage("Heavy", "AM_Heavy"),
        montage("GuardBreak", "AM_GuardBreak"), montage("Dodge", "AM_Dodge"),
        montage("HitReact", "AM_HitReact"), montage("HeavyHitReact", "AM_HitReact_Heavy"),
        montage("GuardBroken", "AM_GuardBroken"), montage("Block", "AM_Block"),
        "!DeathMontagePaths=ClearArray",
    ] + ["+DeathMontagePaths=" + path("AM_Death_0%d" % i) for i in range(1, 5)]
    log("%s: %d public and %d Mixamo assets retargeted; grip from %s" % (name, len(public), len(mixamo), slash))
    log("CONFIG " + name + chr(10) + chr(10).join(lines))

log("done")
