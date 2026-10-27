"""Makes the placeholder combat montages from the Third Person mannequin pack's animations.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_anims.py" -unattended -nosplash

Config/DefaultGame.ini points the characters at these montages.
"""
import unreal

SRC = "/Game/Characters/Mannequins/Anims/"
DST = "/Game/Conquer/Animations"

# Montage name -> (source animation, hold the last frame instead of blending out)
MONTAGES = {
    "AM_Light_Left": ("Unarmed/Attack/MM_Attack_01", False),
    "AM_Light_Right": ("Unarmed/Attack/MM_Attack_02", False),
    "AM_Light_Top": ("Unarmed/Attack/MM_Attack_03", False),
    "AM_Heavy": ("Unarmed/Attack/MM_ChargedAttack", False),
    "AM_GuardBreak": ("Unarmed/Attack/MM_Attack_03", False),
    "AM_Dodge": ("Unarmed/Jump/MM_Dash", False),
    "AM_HitReact": ("Rifle/HitReact/MM_HitReact_Front_Lgt_01", False),
    "AM_HitReact_Heavy": ("Rifle/HitReact/MM_HitReact_Front_Med_01", False),
    "AM_GuardBroken": ("Rifle/HitReact/MM_HitReact_Front_Hvy_01", False),
    "AM_Death_01": ("Death/MM_Death_Front_01", True),
    "AM_Death_02": ("Death/MM_Death_Front_02", True),
    "AM_Death_03": ("Death/MM_Death_Front_03", True),
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

for name, (src, hold) in MONTAGES.items():
    path = DST + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)

    seq = assets.load_asset(SRC + src)
    if not seq:
        raise RuntimeError("missing animation " + SRC + src)

    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", seq)
    factory.set_editor_property("target_skeleton", seq.get_editor_property("skeleton"))
    montage = tools.create_asset(name, DST, unreal.AnimMontage, factory)
    if not montage:
        raise RuntimeError("could not create " + path)
    if hold:
        montage.set_editor_property("enable_auto_blend_out", False)
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log("[build_anims] %s <- %s (%.2fs)" % (path, src, montage.get_play_length()))

unreal.log("[build_anims] done")
