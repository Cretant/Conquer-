"""Builds /Game/Maps/KeepHall, Sir Garrick Vane's fight: a roofed stone hall lit by braziers and torches, with pillars and banners,
run by ConquerBossGameMode (no waves, just the boss). Also makes the glowing material for the burning patches.

Run from the repo root after Tools/build_sword.py (it reuses the placeholder material). To rebuild, delete
Content/Maps/KeepHall.umap first (the editor won't replace a map it has loaded), then:
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
EXPOSURE = 14.0    # auto exposure held at one scene luminance (higher reads darker)


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

# A roof: the hall is lit from within, by fire, not by a shaft of sun on the flagstones.
block("Cube", (0, 0, WALL_HEIGHT + 25), (HALF / 50, HALF / 50, 0.5), stone, "Roof")


def warm_light(location, intensity, radius, label):
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    light.set_actor_label(label)
    lc = light.point_light_component
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("light_color", unreal.Color(r=255, g=170, b=100, a=255))
    lc.set_editor_property("attenuation_radius", radius)
    return light


# Braziers: iron bowls on stone plinths, each with its glow and a light.
for i, (x, y) in enumerate(((-700, 600), (-700, -600), (700, 600), (700, -600))):
    block("Cylinder", (x, y, 45), (0.6, 0.6, 0.9), stone, "Brazier%d" % i)
    block("Cylinder", (x, y, 100), (1.0, 1.0, 0.2), iron, "BrazierBowl%d" % i)
    block("Cylinder", (x, y, 112), (0.8, 0.8, 0.04), fire, "BrazierCoals%d" % i)
    warm_light((x, y, 190), 2500.0, 1400.0, "BrazierLight%d" % i)

# Torches in iron brackets along the long walls, between the banners, and two by the lord's banner.
torches = [(x, side * (HALF - 70)) for side in (-1, 1) for x in (-1200, -300, 300, 1200)]
torches += [(HALF - 70, -500), (HALF - 70, 500)]
for i, (x, y) in enumerate(torches):
    block("Cube", (x, y, 330), (0.12, 0.12, 0.5), iron, "Torch%d" % i)
    block("Sphere", (x, y, 365), (0.14, 0.14, 0.2), fire, "TorchFlame%d" % i)
    inward = unreal.Vector(-x, -y, 0)
    inward.normalize()
    warm_light((x + inward.x * 60, y + inward.y * 60, 370), 900.0, 1100.0, "TorchLight%d" % i)

# A faint cool fill so the stone in shadow still reads (Lumen bounces the firelight on top).
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 400), unreal.Rotator(0, 0, 0))
sky.set_actor_label("AmbientFill")
sky.light_component.set_editor_property("source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
sky.light_component.set_editor_property("cubemap", unreal.load_object(None, "/Engine/MapTemplates/Sky/SunsetAmbientCubemap.SunsetAmbientCubemap"))
sky.light_component.set_editor_property("intensity", 0.35)
sky.light_component.set_editor_property("lower_hemisphere_is_black", False)
# No height fog: inside the walls its blue in-scatter floods the whole hall.

# Exposure held still, so the eye doesn't adapt the dimness away or blow the fire out.
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
post.set_actor_label("PostProcess")
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
for name, value in (("auto_exposure_min_brightness", EXPOSURE), ("auto_exposure_max_brightness", EXPOSURE),
                    ("auto_exposure_bias", 0.0)):
    settings.set_editor_property("override_" + name, True)
    settings.set_editor_property(name, value)
post.set_editor_property("settings", settings)

# Wren starts at the door end, facing down the hall; Garrick walks in at the far end.
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-HALF + 350, 0, 100), unreal.Rotator(0, 0, 0)).set_actor_label("PlayerStart")

levels.save_current_level()
log("saved %s (%s); burning patch material %s" % (MAP, "ConquerBossGameMode", fire.get_path_name()))
