"""Makes the combat montages from the sword clips Tools/build_sword.py retargets (run that first), plus deaths
from the Third Person mannequin pack: a public CC0 set, and a local Mixamo set where Mixamo's pack is imported.

Each attack gets a Hit notify (UConquerHitNotify) on its contact frame: the moment the striking hand (or foot, for
a kick) moves fastest. The combat component times the montage so that frame lands when the blow resolves.

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
    # Sir Garrick's: the phase-two roar, kneeling for the execution, and the shield bash.
    "AM_Roar": (SWORD + "A_Hit_Knockback", False),
    "AM_Kneel": (SWORD + "A_Fixing_Kneeling", True),
    "AM_ShieldBash": (SWORD + "A_Melee_Hook", False),
    # Wren's light combo (out of lock-on each light is the next of these) and heavy combo.
    "AM_Combo_1": (SWORD + "A_Sword_Regular_A", False),
    "AM_Combo_2": (SWORD + "A_Sword_Regular_B", False),
    "AM_Combo_3": (SWORD + "A_Sword_Regular_C", False),
    "AM_Combo_4": (SWORD + "A_Sword_Attack", False),
    "AM_HeavyCombo_1": (SWORD + "A_Sword_Heavy_Combo", False),
    "AM_HeavyCombo_2": (SWORD + "A_Sword_Regular_C", False),
    # The dodge is a roll; a light out of it, a dashing slash.
    "AM_DodgeRoll": (SWORD + "A_Roll", False),
    "AM_DodgeAttack": (SWORD + "A_Sword_Dash", False),
    # A parry: her deflection, and the attacker knocked back.
    "AM_Parry": (SWORD + "A_Sword_Block", False),
    "AM_ParriedRecoil": (SWORD + "A_Hit_Knockback", False),
    # The pitchfork jabs instead of swinging.
    "AM_Thrust_Light": (SWORD + "A_Punch_Jab", False),
    "AM_Thrust_Heavy": (SWORD + "A_Punch_Cross", False),
    # Archers: drawing and holding the aim, then the loose.
    "AM_BowDraw": (SWORD + "A_Pistol_Aim_Neutral", False),
    "AM_BowRelease": (SWORD + "A_Pistol_Shoot", False),
    # The opening: getting up off the ground; the cutscene soldiers carry torches.
    "AM_GetUp": (SWORD + "A_LayToIdle", False),
    "AM_TorchIdle": (SWORD + "A_Idle_Torch_Loop", False),
    # Executions: a flurry to finish.
    "AM_Execution_1": (SWORD + "A_Sword_Regular_Combo", False),
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
    "AM_Roar": (MX + "A_MX_Power_Up", False),
    "AM_ShieldBash": (MX + "A_MX_Attack_3", False),
    "AM_Combo_1": (MX + "A_MX_Slash", False),
    "AM_Combo_2": (MX + "A_MX_Slash_2", False),
    "AM_Combo_3": (MX + "A_MX_Slash_4", False),
    "AM_Combo_4": (MX + "A_MX_Attack", False),
    "AM_HeavyCombo_1": (MX + "A_MX_Slash_3", False),
    "AM_HeavyCombo_2": (MX + "A_MX_Slash_5", False),
    "AM_Parry": (MX + "A_MX_Block_2", False),
    "AM_ParriedRecoil": (MX + "A_MX_Impact_3", False),
    "AM_Execution_1": (MX + "A_MX_Slash_5", False),
})

# Attacks get a Hit notify on their contact frame, found from the speed of the bone that strikes.
ATTACKS = {"AM_Light_Left": "hand_r", "AM_Light_Right": "hand_r", "AM_Light_Top": "hand_r", "AM_Heavy": "hand_r",
           "AM_GuardBreak": "hand_r", "AM_ShieldBash": "hand_l", "AM_Combo_1": "hand_r", "AM_Combo_2": "hand_r",
           "AM_Combo_3": "hand_r", "AM_Combo_4": "hand_r", "AM_HeavyCombo_1": "hand_r", "AM_HeavyCombo_2": "hand_r",
           "AM_DodgeAttack": "hand_r", "AM_Thrust_Light": "hand_r", "AM_Thrust_Heavy": "hand_r", "AM_Execution_1": "hand_r"}
KICKS = {"A_MX_Kick": "foot_r"}


def contact_time(seq, bone):
    """When the striking bone moves fastest, between a fifth and four fifths of the way through."""
    length = seq.get_play_length()
    opts = unreal.AnimPoseEvaluationOptions()
    poses = unreal.AnimPoseExtensions
    step = 1.0 / 60.0
    times = [i * step for i in range(int(length / step) + 1)]
    points = [poses.get_bone_pose(poses.get_anim_pose_at_time(seq, t, opts), bone, unreal.AnimPoseSpaces.WORLD).translation
              for t in times]
    best, best_t = -1.0, length * 0.4
    for i in range(1, len(points)):
        t = times[i]
        if t < length * 0.2 or t > length * 0.8:
            continue
        speed = (points[i] - points[i - 1]).length() / step
        if speed > best:
            best, best_t = speed, t
    return best_t

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
    if name in ATTACKS:
        bone = KICKS.get(src.split("/")[-1], ATTACKS[name])
        hit = contact_time(seq, bone)
        unreal.AnimationLibrary.add_animation_notify_track(montage, "Hits")
        unreal.AnimationLibrary.add_animation_notify_event(montage, "Hits", hit, unreal.ConquerHitNotify)
        unreal.log("[build_anims] %s hits at %.2fs (%s)" % (name, hit, bone))
    assets.save_asset(path, only_if_is_dirty=False)
    unreal.log("[build_anims] %s <- %s (%.2fs)" % (path, src, montage.get_play_length()))

unreal.log("[build_anims] done")
