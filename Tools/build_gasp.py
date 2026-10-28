"""Wren's locomotion from Epic's Game Animation Sample: eight-way walks and runs, starts, stops on either foot, turns
on the spot, idle breaks and a sprint, retargeted onto her (and any other character in TARGETS).

The sample's content is Epic's, under the Unreal Engine EULA: this project may use it, but it can't go in the public
repo, so everything this makes is local only (Content/ThirdParty and Content/Characters/Conquer/*/GASP are
gitignored). Without it the characters keep the mannequin's clips. The clips come as FBX exported from the sample
project into GASP_EXPORT (set with the environment variable CONQUER_GASP_EXPORT).

Run from the repo root after Tools/build_characters.py (it reuses that script's IK rigs for the targets):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_gasp.py" -unattended -nosplash
"""
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GASP_EXPORT = os.environ.get("CONQUER_GASP_EXPORT", r"D:/Claudi/Saved/ThirdParty/GameAnimationSample")
DIR = "/Game/ThirdParty/GASP"
CHARS = "/Game/Characters/Conquer"
TARGETS = ["Wren"]

# Neutral (unarmed) clips: loops every eighth of the way round, starts, stops on either foot, turns, idle breaks.
CLIPS = ["M_Neutral_Stand_Idle_Loop", "M_Neutral_Stand_Idle_Break_v01", "M_Neutral_Stand_Idle_Break_v02",
         "M_Neutral_Stand_Idle_Break_v03", "M_Neutral_Sprint_Loop_F",
         "M_Neutral_Walk_Start_F_Lfoot", "M_Neutral_Run_Start_F_Lfoot",
         "M_Neutral_Stand_Turn_090_L", "M_Neutral_Stand_Turn_090_R", "M_Neutral_Stand_Turn_180_L", "M_Neutral_Stand_Turn_180_R"]
for gait in ("Walk", "Run"):
    CLIPS += ["M_Neutral_%s_Loop_%s" % (gait, d) for d in ("F", "FL", "FR", "LL", "RR", "BL", "BR", "B")]
    CLIPS += ["M_Neutral_%s_Stop_%s_%sfoot" % (gait, d, foot) for d in ("F", "B", "LL", "RR") for foot in ("L", "R")]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log("[build_gasp] " + msg)


def wipe(game_dir):
    shutil.rmtree(os.path.join(REPO, "Content", game_dir[len("/Game/"):]), ignore_errors=True)


def import_fbx(path, destination, skeleton=None):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = destination
    task.replace_existing = True
    task.automated = True
    task.save = True
    options = unreal.FbxImportUI()
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    if skeleton is None:
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    else:
        options.set_editor_property("import_mesh", False)
        options.set_editor_property("import_animations", True)
        options.set_editor_property("skeleton", skeleton)
        options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
        anim = options.get_editor_property("anim_sequence_import_data")
        anim.set_editor_property("import_bone_tracks", True)
        anim.set_editor_property("remove_redundant_keys", False)
    task.options = options
    tools.import_asset_tasks([task])


def make_ik_rig(name, folder, mesh):
    rig = tools.create_asset(name, folder, unreal.IKRigDefinition, unreal.IKRigDefinitionFactory())
    ctrl = unreal.IKRigController.get_controller(rig)
    ctrl.set_skeletal_mesh(mesh)
    ctrl.apply_auto_generated_retarget_definition()
    assets.save_asset(rig.get_path_name(), only_if_is_dirty=False)
    return rig


def make_retargeter(name, source_rig, target_rig, source_mesh, target_mesh):
    rtg = tools.create_asset(name, DIR, unreal.IKRetargeter, unreal.IKRetargetFactory())
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


if not os.path.isdir(os.path.join(GASP_EXPORT, "Anims")):
    raise RuntimeError("no Game Animation Sample export at %s (set CONQUER_GASP_EXPORT)" % GASP_EXPORT)

wipe(DIR)
for name in TARGETS:
    wipe("%s/%s/GASP" % (CHARS, name))

import_fbx(os.path.join(GASP_EXPORT, "SKM_UEFN_Mannequin.fbx"), DIR)
source_mesh = assets.load_asset(DIR + "/SKM_UEFN_Mannequin")
if not source_mesh:
    raise RuntimeError("the sample's mannequin didn't import")
skeleton = source_mesh.get_editor_property("skeleton")
for clip in CLIPS:
    import_fbx(os.path.join(GASP_EXPORT, "Anims", clip + ".fbx"), DIR + "/Anims", skeleton)
clips = [assets.find_asset_data("%s/Anims/%s" % (DIR, c)) for c in CLIPS]
missing = [c for c, a in zip(CLIPS, clips) if not a.is_valid()]
if missing:
    raise RuntimeError("these clips didn't import: %s" % missing)
source_rig = make_ik_rig("IK_UEFN_Mannequin", DIR, source_mesh)

for name in TARGETS:
    folder = "%s/%s" % (CHARS, name)
    meshes = [a for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(folder, recursive=False)
              if str(a.asset_class_path.asset_name) == "SkeletalMesh"]
    target_rig = assets.load_asset("%s/Retarget/IK_%s" % (CHARS, name))
    if not meshes or not target_rig:
        log("%s: no mesh or IK rig (run Tools/build_characters.py), skipped" % name)
        continue
    mesh = meshes[0].get_asset()
    rtg = make_retargeter("RTG_GASP_To_" + name, source_rig, target_rig, source_mesh, mesh)

    inputs = unreal.IKRetargetBatchOperationInputs()
    inputs.assets_to_retarget = clips
    inputs.source_mesh = source_mesh
    inputs.target_mesh = mesh
    inputs.ik_retarget_asset = rtg
    inputs.target_path = folder + "/GASP"
    inputs.suffix = "_" + name
    inputs.include_referenced_assets = False
    inputs.overwrite_existing_files = False
    made = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
    for a in made:
        asset = a.get_asset()
        if isinstance(asset, unreal.AnimSequence):
            # The characters move the capsule themselves; the clips play in place (the root held to its rest pose).
            asset.set_editor_property("enable_root_motion", True)
            asset.set_editor_property("root_motion_root_lock", unreal.RootMotionRootLock.REF_POSE)
        assets.save_asset(str(a.package_name), only_if_is_dirty=False)
    log("%s: %d clips retargeted into %s/GASP" % (name, len(made), folder))

log("done")
