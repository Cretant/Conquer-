"""Builds /Game/Maps/KeepHall, Sir Garrick Vane's fight: an enclosed stone hall with pillars, banners and braziers,
run by ConquerBossGameMode (no waves, just the boss). Also makes the glowing material for the burning patches.

Run from the repo root after Tools/build_sword.py (it reuses the placeholder material). Re-running replaces it:
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_keephall.py" -unattended -nosplash
"""
import os
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDER = "/Game/Conquer/KeepHall"
MAP = "/Game/Maps/KeepHall"
PLACEHOLDER = "/Game/Conquer/Weapons/M_Placeholder"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary

HALF = 1500.0      # the hall is 3000 x 3000
WALL_HEIGHT = 700.0


def log(msg):
    unreal.log("[build_keephall] " + msg)


def tinted(name, color, metallic=0.0, roughness=0.8):
    mi = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", assets.load_asset(PLACEHOLDER))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    assets.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    return mi


def glowing(name, color, strength):
    """Unlit-looking glow: dark base colour, strong emissive."""
    mat = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 0)
    glow.set_editor_property("constant", unreal.LinearColor(*[c * strength for c in color], 1))
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 200)
    base.set_editor_property("constant", unreal.LinearColor(0.05, 0.02, 0.01, 1))
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(mat)
    assets.save_asset(mat.get_path_name(), only_if_is_dirty=False)
    return mat


def block(shape, location, scale, material, label, rotation=unreal.Rotator(0, 0, 0)):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
    actor.set_actor_label(label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(assets.load_asset("/Engine/BasicShapes/" + shape))
    comp.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


# Clear the last build on disk before the editor loads it (deleting in the editor leaves the map in place).
shutil.rmtree(os.path.join(REPO, "Content", FOLDER[len("/Game/"):]), ignore_errors=True)
stone = tinted("MI_Stone", (0.30, 0.28, 0.26), 0.0, 0.9)
floor_mat = tinted("MI_Flagstones", (0.22, 0.20, 0.18), 0.0, 0.85)
banner = tinted("MI_Banner", (0.42, 0.03, 0.02), 0.0, 0.7)
gold = tinted("MI_Gold", (0.55, 0.40, 0.12), 1.0, 0.4)
iron = tinted("MI_Iron", (0.12, 0.12, 0.13), 1.0, 0.5)
fire = glowing("M_BurningPatch", (1.0, 0.22, 0.03), 4.0)

if assets.does_asset_exist(MAP):
    assets.delete_asset(MAP)
if assets.does_asset_exist(MAP):
    raise RuntimeError("delete Content/Maps/KeepHall.umap before running this (the editor won't replace a loaded map)")
levels.new_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/Conquer.ConquerBossGameMode"))

# Floor and walls (the basic cube is 100 units on a side, centred).
block("Cube", (0, 0, -50), (HALF / 50, HALF / 50, 1), floor_mat, "Floor")
for i, (x, y, sx, sy) in enumerate(((HALF, 0, 1, HALF / 50), (-HALF, 0, 1, HALF / 50), (0, HALF, HALF / 50, 1), (0, -HALF, HALF / 50, 1))):
    block("Cube", (x, y, WALL_HEIGHT / 2), (sx, sy, WALL_HEIGHT / 100), stone, "Wall%d" % i)

# Two rows of pillars down the hall, banners between them on the long walls.
for side in (-1, 1):
    for i, x in enumerate((-900, -300, 300, 900)):
        block("Cube", (x, side * 950, WALL_HEIGHT / 2), (1.3, 1.3, WALL_HEIGHT / 100), stone, "Pillar%d_%d" % (side, i))
    for i, x in enumerate((-600, 0, 600)):
        block("Cube", (x, side * (HALF - 60), 380), (1.6, 0.06, 3.6), banner, "Banner%d_%d" % (side, i))
        block("Cube", (x, side * (HALF - 66), 560), (1.8, 0.08, 0.12), gold, "BannerRod%d_%d" % (side, i))
# The lord's banner behind where Garrick stands.
block("Cube", (HALF - 60, 0, 400), (0.06, 3.0, 5.0), banner, "LordBanner")

# Braziers: iron bowls on stone plinths, with a glow and a light.
for i, (x, y) in enumerate(((-700, 600), (-700, -600), (700, 600), (700, -600))):
    block("Cylinder", (x, y, 45), (0.6, 0.6, 0.9), stone, "Brazier%d" % i)
    block("Cylinder", (x, y, 100), (1.0, 1.0, 0.2), iron, "BrazierBowl%d" % i)
    block("Cylinder", (x, y, 112), (0.8, 0.8, 0.04), fire, "BrazierCoals%d" % i)
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(x, y, 180), unreal.Rotator(0, 0, 0))
    light.set_actor_label("BrazierLight%d" % i)
    lc = light.point_light_component
    lc.set_editor_property("intensity", 1200.0)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=140, b=60, a=255))
    lc.set_editor_property("attenuation_radius", 700.0)

# Late daylight falling steeply through the open roof onto the floor (a low sun leaves the hall in the walls'
# shadow, lit only by the blue sky, and auto exposure blows it out), and a faint sky.
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 900), unreal.Rotator(0, -72, -30))
sun.set_actor_label("Sun")
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sun.light_component.set_editor_property("intensity", 10.0)
sun.light_component.set_editor_property("light_color", unreal.Color(r=255, g=220, b=180, a=255))
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, 0, 0))
sky.set_actor_label("SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_editor_property("intensity", 0.25)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
# No height fog: inside the walls its blue in-scatter floods the whole hall.
# Exposure held a little down, or the sunlit flagstones blow out to white.
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
post.set_actor_label("PostProcess")
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_bias", True)
settings.set_editor_property("auto_exposure_bias", -1.5)
post.set_editor_property("settings", settings)

# Wren starts at the door end, facing down the hall; Garrick walks in at the far end.
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-HALF + 350, 0, 100), unreal.Rotator(0, 0, 0)).set_actor_label("PlayerStart")

levels.save_current_level()
log("saved %s (%s); burning patch material %s" % (MAP, "ConquerBossGameMode", fire.get_path_name()))
