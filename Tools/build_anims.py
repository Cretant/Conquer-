"""Makes the combat montages from the sword clips Tools/build_sword.py retargets (run that first), plus deaths
from the Third Person mannequin pack: a public CC0 set, and a local Mixamo set where Mixamo's pack is imported.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_anims.py" -unattended -nosplash

Config/DefaultGame.ini points the characters at these montages.
"""
import unreal

SWORD = "/Game/Conquer/Animations/Sword/"
MX = "/Game/Conquer/Animations/Mixamo/Clips/"
PACK = "/Game/Characters/Mannequins/Anims/"
DST = "/Game/Conquer/Animations"
DST_MIXAMO = DST + "/Mixamo"

# Montage name -> (source animation, hold the last frame instead of blending out).
# The public set uses Quaternius' CC0 clips; it is what a fresh clone of the repo plays.
PUBLIC = {
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
    "AM_Death_01": (SWORD + "A_Death01", True),
    "AM_Death_02": (PACK + "Death/MM_Death_Front_01", True),
    "AM_Death_03": (PACK + "Death/MM_Death_Front_02", True),
    "AM_Death_04": (PACK + "Death/MM_Death_Front_03", True),
}
# The same montages from Mixamo's Sword And Shield Pack, built only where that pack has been imported. They stay
# out of git; the game prefers them when they exist (see LocalAnimOverrides in Config/DefaultGame.ini).
MIXAMO = dict(PUBLIC, **{
    "AM_Light_Left": (MX + "A_MX_Slash", False),
    "AM_Light_Right": (MX + "A_MX_Attack_2", False),
    "AM_Light_Top": (MX + "A_MX_Attack_4", False),
    "AM_Heavy": (MX + "A_MX_Slash_3", False),
    "AM_GuardBreak": (MX + "A_MX_Kick", False),
    "AM_Block": (MX + "A_MX_Block", False),
    "AM_HitReact": (MX + "A_MX_Impact", False),
    "AM_HitReact_Heavy": (MX + "A_MX_Impact_2", False),
    "AM_GuardBroken": (MX + "A_MX_Impact_3", False),
    "AM_Death_01": (MX + "A_MX_Death", True),
    "AM_Death_02": (MX + "A_MX_Death_2", True),
})

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

SETS = [(DST, PUBLIC)]
if unreal.EditorAssetLibrary.does_asset_exist(MX + "A_MX_Slash"):
    SETS.append((DST_MIXAMO, MIXAMO))

for dst, name, (src, hold) in [(d, n, v) for d, table in SETS for n, v in table.items()]:
    path = dst + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)

    seq = assets.load_asset(src)
    if not seq:
        raise RuntimeError("missing animation " + src)

    factory = unreal.AnimMontageFactory()
    factory.set_editor_property("source_animation", seq)
    factory.set_editor_property("target_skeleton", seq.get_editor_property("skeleton"))
    montage = tools.create_asset(name, dst, unreal.AnimMontage, factory)
    if not montage:
        raise RuntimeError("could not create " + path)
    if hold:
        montage.set_editor_property("enable_auto_blend_out", False)
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log("[build_anims] %s <- %s (%.2fs)" % (path, src, montage.get_play_length()))

unreal.log("[build_anims] done")
