"""Makes the combat montages: sword moves from Quaternius' Universal Animation Library (retargeted by
Tools/build_sword.py, run that first) plus deaths from the Third Person mannequin pack.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_anims.py" -unattended -nosplash

Config/DefaultGame.ini points the characters at these montages.
"""
import unreal

SWORD = "/Game/Conquer/Animations/Sword/"
PACK = "/Game/Characters/Mannequins/Anims/"
DST = "/Game/Conquer/Animations"

# Montage name -> (source animation, hold the last frame instead of blending out)
MONTAGES = {
    "AM_Light_Left": (SWORD + "A_Sword_Regular_A", False),
    "AM_Light_Right": (SWORD + "A_Sword_Regular_B", False),
    "AM_Light_Top": (SWORD + "A_Sword_Attack", False),
    "AM_Heavy": (SWORD + "A_Sword_Regular_C", False),
    "AM_GuardBreak": (SWORD + "A_Melee_Hook", False),
    "AM_Block": (SWORD + "A_Sword_Block", False),
    "AM_Dodge": (SWORD + "A_Sword_Dash", False),
    "AM_HitReact": (SWORD + "A_Hit_Chest", False),
    "AM_HitReact_Heavy": (SWORD + "A_Hit_Head", False),
    "AM_GuardBroken": (SWORD + "A_Hit_Knockback", False),
    "AM_Death_01": (PACK + "Death/MM_Death_Front_01", True),
    "AM_Death_02": (PACK + "Death/MM_Death_Front_02", True),
    "AM_Death_03": (PACK + "Death/MM_Death_Front_03", True),
    "AM_Death_04": (SWORD + "A_Death01", True),
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

for name, (src, hold) in MONTAGES.items():
    path = DST + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)

    seq = assets.load_asset(src)
    if not seq:
        raise RuntimeError("missing animation " + src)

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
