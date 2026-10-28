"""Imports the Meshy models into Unreal: the characters from Art/Meshy/<Name>/ into /Game/Characters/Conquer/<Name>,
and the sword and shield, as prepared for the hands by Tools/prep_props.py (Art/Weapons/SM_Meshy*.fbx), into
/Game/Props/<Name>.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/import_meshy.py" -unattended -nosplash

The models are not used by the game yet: the mannequins stay until they are reviewed.
"""
import glob
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(REPO, "Art", "Meshy")

MODELS = {
    "Wren": "/Game/Characters/Conquer/Wren",
    "Soldier": "/Game/Characters/Conquer/Soldier",
    "Garrick": "/Game/Characters/Conquer/Garrick",
}
PROPS = {  # name -> (prepared FBX in Art/Weapons, destination, Meshy download folder with the PBR maps)
    "Sword": ("SM_MeshySword.fbx", "/Game/Props/Sword", "Sword2"),
    "Shield": ("SM_MeshyShield.fbx", "/Game/Props/Shield", "Shield"),
}

tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def log(msg):
    unreal.log("[import_meshy] " + msg)


MATERIALS = "/Game/Conquer/Materials"
MASTER = MATERIALS + "/M_MeshyPBR"
TEXTURE_KINDS = {  # file suffix -> (material parameter, sRGB, compression)
    "_metallic.png": ("Metallic", False, unreal.TextureCompressionSettings.TC_MASKS),
    "_roughness.png": ("Roughness", False, unreal.TextureCompressionSettings.TC_MASKS),
    "_normal.png": ("Normal", False, unreal.TextureCompressionSettings.TC_NORMALMAP),
    ".png": ("BaseColor", True, unreal.TextureCompressionSettings.TC_DEFAULT),
}


def kind_of(filename):
    if filename.endswith("_metallic_roughness.png"):
        return None
    return next((k for k in TEXTURE_KINDS if filename.endswith(k)), None)


def make_master(sample):
    """A plain PBR material: base colour, normal, metallic and roughness maps (the Interchange import only made a
    legacy Phong material with no metallic, which left the armour chalky white)."""
    if unreal.EditorAssetLibrary.does_asset_exist(MASTER):
        unreal.EditorAssetLibrary.delete_asset(MASTER)
    mel = unreal.MaterialEditingLibrary
    mat = tools.create_asset("M_MeshyPBR", MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    # A standalone game can't add usages at runtime, and without this flag skinned meshes fall back to grey.
    mat.set_editor_property("used_with_skeletal_mesh", True)
    wiring = [("BaseColor", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", unreal.MaterialProperty.MP_BASE_COLOR),
              ("Normal", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "RGB", unreal.MaterialProperty.MP_NORMAL),
              ("Metallic", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", unreal.MaterialProperty.MP_METALLIC),
              ("Roughness", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", unreal.MaterialProperty.MP_ROUGHNESS)]
    for i, (param, sampler, output, prop) in enumerate(wiring):
        node = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -500, i * 260)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler)
        node.set_editor_property("texture", sample[param])
        mel.connect_material_property(node, output, prop)
    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MASTER, only_if_is_dirty=False)
    return mat


def import_pbr_textures(name, art_folder, dest):
    """Imports Meshy's own PBR maps (Meshy_AI_*.png) for one model into <dest>/Textures."""
    found = {}
    for path in sorted(glob.glob(os.path.join(art_folder, "**", "Meshy_AI_*.png"), recursive=True)):
        kind = kind_of(os.path.basename(path))
        if not kind or TEXTURE_KINDS[kind][0] in found:
            continue
        param, srgb, compression = TEXTURE_KINDS[kind]
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = dest + "/Textures"
        task.destination_name = "T_%s_%s" % (name, param)
        task.replace_existing = True
        task.automated = True
        task.save = False
        tools.import_asset_tasks([task])
        tex = unreal.EditorAssetLibrary.load_asset("%s/Textures/T_%s_%s" % (dest, name, param))
        tex.set_editor_property("srgb", srgb)
        tex.set_editor_property("compression_settings", compression)
        unreal.EditorAssetLibrary.save_asset(tex.get_path_name(), only_if_is_dirty=False)
        found[param] = tex
    return found


def apply_pbr(name, art_folder, dest, mesh, master_holder):
    textures = import_pbr_textures(name, art_folder, dest)
    if len(textures) < 4:
        log("%s: only found %s maps, kept the imported material" % (name, sorted(textures)))
        return
    if master_holder[0] is None:
        master_holder[0] = make_master(textures)
    mi = tools.create_asset("MI_" + name, dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", master_holder[0])
    for param, tex in textures.items():
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, tex)
    unreal.EditorAssetLibrary.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    if isinstance(mesh, unreal.SkeletalMesh):
        # The array hands back copies, so build a new one rather than editing its elements in place.
        mesh.modify()
        mesh.set_editor_property("materials", [
            unreal.SkeletalMaterial(material_interface=mi, material_slot_name=m.get_editor_property("material_slot_name"))
            for m in mesh.get_editor_property("materials")])
    else:
        for i in range(len(mesh.get_editor_property("static_materials"))):
            mesh.set_material(i, mi)
    unreal.EditorAssetLibrary.save_asset(mesh.get_path_name(), only_if_is_dirty=False)
    # Drop the legacy material and its textures that the FBX import made.
    for a in registry.get_assets_by_path(dest, recursive=False):
        cls = str(a.asset_class_path.asset_name)
        if cls in ("MaterialInstanceConstant", "Material", "Texture2D") and not str(a.asset_name).startswith(("MI_", "T_")):
            unreal.EditorAssetLibrary.delete_asset(str(a.package_name))
    log("%s: PBR material %s with %s" % (name, mi.get_path_name(), sorted(textures)))


master = [None]
for name, dest in MODELS.items():
    # Meshy's zips hold one folder; rigged downloads have a *_Character_output.fbx next to an animation FBX.
    files = (sorted(glob.glob(os.path.join(ART, name, name + "_rig.fbx")))  # cleaned by Tools/fix_meshy_rigs.py
             or sorted(glob.glob(os.path.join(ART, name, "**", "*Character_output.fbx"), recursive=True))
             or sorted(glob.glob(os.path.join(ART, name, "**", "*.fbx"), recursive=True))
             or sorted(glob.glob(os.path.join(ART, name, "**", "*.glb"), recursive=True)))
    if not files:
        log("%s: no FBX or GLB in Art/Meshy/%s, skipped" % (name, name))
        continue
    # Start clean on disk: the commandlet's asset registry may not know about the last run's packages.
    shutil.rmtree(os.path.join(REPO, "Content", dest[len("/Game/"):]), ignore_errors=True)

    task = unreal.AssetImportTask()
    task.filename = files[0]
    task.destination_path = dest
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])

    found = registry.get_assets_by_path(dest, recursive=True)
    kinds = sorted(set(str(a.asset_class_path.asset_name) for a in found))
    log("%s: %s -> %s, %d assets (%s)" % (name, os.path.basename(files[0]), dest, len(found), ", ".join(kinds)))
    for a in found:
        if str(a.asset_class_path.asset_name) in ("SkeletalMesh", "StaticMesh"):
            mesh = a.get_asset()
            box = mesh.get_bounds().box_extent if hasattr(mesh, "get_bounds") else None
            log("  %s %s, half-size %s" % (a.asset_class_path.asset_name, a.asset_name, box))
            apply_pbr(name, os.path.join(ART, name), dest, mesh, master)

for name, (fbx, dest, maps) in PROPS.items():
    path = os.path.join(REPO, "Art", "Weapons", fbx)
    if not os.path.exists(path):
        log("%s: %s missing, skipped (run Tools/prep_props.py)" % (name, fbx))
        continue
    shutil.rmtree(os.path.join(REPO, "Content", dest[len("/Game/"):]), ignore_errors=True)
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = dest
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    mesh = next(a.get_asset() for a in registry.get_assets_by_path(dest, recursive=True)
                if str(a.asset_class_path.asset_name) == "StaticMesh")
    box = mesh.get_bounding_box()
    log("%s: %s -> %s, bounds %s .. %s" % (name, fbx, mesh.get_path_name(), box.min, box.max))
    apply_pbr(name, os.path.join(ART, maps), dest, mesh, master)

log("done")
