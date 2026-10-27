"""Builds the sword content: Quaternius' Universal Animation Library 1 and 2 (CC0) retargeted onto the UE5
mannequin, and the placeholder sword mesh (Art/Weapons/SM_Sword.fbx, made by Tools/make_sword.py).

Needs the Standard zips extracted under Art/Quaternius (see Art/LICENSES.md). Run from the repo root:
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_sword.py" -unattended -nosplash

Retargeted clips land in /Game/Conquer/Animations/Sword as A_<clip>, e.g. A_Sword_Regular_A. Tools/build_anims.py makes the montages.
"""
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(REPO, "Art", "Quaternius")
OUT = "/Game/Conquer/Animations/Sword"
RIGS = "/Game/Conquer/Animations/Retarget"
TARGET_MESH = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"

PACKS = {
    "UAL1": ("Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb",
             ["Sword_Attack", "Sword_Idle", "Hit_Chest", "Hit_Head", "Death01", "Roll"]),
    "UAL2": ("Universal Animation Library 2[Standard]/Unreal-Godot/UAL2_Standard.glb",
             ["Sword_Regular_A", "Sword_Regular_B", "Sword_Regular_C", "Sword_Heavy_Combo", "Sword_Block",
              "Sword_Dash", "Hit_Knockback", "Melee_Hook"]),
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
    dest = "/Game/Quaternius/" + name
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
    assets.save_asset(path, only_if_is_dirty=False)
    return rtg


def short_name(asset_data):
    # Interchange prefixes each clip with the file name, e.g. UAL1_StandardSword_Attack.
    return str(asset_data.asset_name)


WEAPONS = "/Game/Conquer/Weapons"

# Where the grip sits in hand_r's space while the UAL fist is closed (measured from the retargeted clips):
# the grip runs from the pinky to the index finger, and the edge faces the way the knuckles point.
GRIP_CENTRE = unreal.Vector(-3.0, -5.8, -3.0)
GRIP_AXIS = unreal.Vector(0.17, -0.38, 0.91)
KNUCKLES = unreal.Vector(0.0, -1.0, 0.0)


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
    edge = KNUCKLES - GRIP_AXIS * KNUCKLES.dot(GRIP_AXIS)
    rot = unreal.MathLibrary.make_rot_from_xy(GRIP_AXIS.normal(), edge.normal())
    log("sword bounds min %s max %s" % (box.min, box.max))
    log("WeaponLocation=(X=%.2f,Y=%.2f,Z=%.2f)" % (GRIP_CENTRE.x, GRIP_CENTRE.y, GRIP_CENTRE.z))
    log("WeaponRotation=(Pitch=%.2f,Yaw=%.2f,Roll=%.2f)" % (rot.pitch, rot.yaw, rot.roll))


for folder in (RIGS, OUT, WEAPONS, "/Game/Quaternius"):
    wipe(folder)

target_mesh = assets.load_asset(TARGET_MESH)
target_rig = make_ik_rig("IK_Mannequin", target_mesh)

for pack, (rel, wanted) in PACKS.items():
    source_mesh, anims = import_pack(pack, rel)
    log("%s clips: %s" % (pack, sorted(short_name(a) for a in anims)))
    source_rig = make_ik_rig("IK_" + pack, source_mesh)
    rtg = make_retargeter("RTG_" + pack + "_To_Mannequin", source_rig, target_rig, source_mesh, target_mesh)

    picked = []
    for clip in wanted:
        match = [a for a in anims if short_name(a) in (clip, pack + "_Standard" + clip)]
        if not match:
            raise RuntimeError("%s has no clip %s" % (pack, clip))
        picked.append(match[0])

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = picked
    inputs.source_mesh = source_mesh
    inputs.target_mesh = target_mesh
    inputs.ik_retarget_asset = rtg
    inputs.target_path = OUT
    inputs.search = pack + "_Standard"
    inputs.replace = ""
    inputs.prefix = "A_"
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = True
    for asset_data in unreal.IKRetargetBatchOperation.run_batch_retarget(inputs):
        asset = asset_data.get_asset()
        if isinstance(asset, unreal.AnimSequence):
            assets.save_asset(asset.get_path_name(), only_if_is_dirty=False)
            log("retargeted %s (%.2fs)" % (asset.get_path_name(), asset.get_play_length()))

build_sword_mesh()
log("done")
