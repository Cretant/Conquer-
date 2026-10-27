"""Builds Conquer's generated content headlessly: the title screen textures, the MainMenu map and the Arena map.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_content.py" -unattended -nosplash
"""
import os

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(msg):
    unreal.log("[build_content] " + msg)


def import_ui_texture(png, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.join(REPO, "Art", "UI", png)
    task.destination_path = "/Game/UI"
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    path = "/Game/UI/" + name
    tex = assets.load_asset(path)
    if not tex:
        raise RuntimeError("import failed: " + task.filename)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("never_stream", True)
    assets.save_asset(path, only_if_is_dirty=False)
    log("imported %s (%dx%d)" % (path, tex.blueprint_get_size_x(), tex.blueprint_get_size_y()))


def new_map(path):
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    if not levels.new_level(path):
        raise RuntimeError("could not create " + path)
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def spawn(cls, location, rotation=unreal.Rotator(0, 0, 0), label=None):
    actor = actors.spawn_actor_from_class(cls, location, rotation)
    if label:
        actor.set_actor_label(label)
    return actor


def build_main_menu():
    world = new_map("/Game/Maps/MainMenu")
    menu_mode = unreal.load_class(None, "/Script/Conquer.ConquerMenuGameMode")
    world.get_world_settings().set_editor_property("default_game_mode", menu_mode)
    levels.save_current_level()
    log("saved /Game/Maps/MainMenu (GameMode override: ConquerMenuGameMode)")


def build_arena():
    new_map("/Game/Maps/Arena")

    # 6000 x 6000 floor whose top sits at Z = 0.
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -50), label="Floor")
    mesh = floor.static_mesh_component
    mesh.set_static_mesh(assets.load_asset("/Engine/BasicShapes/Cube"))
    mesh.set_material(0, assets.load_asset("/Engine/EngineMaterials/WorldGridMaterial"))
    floor.set_actor_scale3d(unreal.Vector(60, 60, 1))

    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, -40, -35), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sun.light_component.set_editor_property("intensity", 8.0)

    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 600), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)

    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
    spawn(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0), label="Fog")
    spawn(unreal.PlayerStart, unreal.Vector(0, 0, 100), label="PlayerStart")

    levels.save_current_level()
    log("saved /Game/Maps/Arena")


import_ui_texture("TitleScreen.png", "T_TitleScreen")
import_ui_texture("Logo.png", "T_Logo")
build_main_menu()
build_arena()
log("done")
