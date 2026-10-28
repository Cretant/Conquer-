"""Builds /Game/Maps/Marrowford, the first level, as a greybox: Wren's farm in the south-west, the lane into the
burning village, the square (with a side path to the smithy), the river and its mill bridge, and the village gate
in the north-east. Run by MarrowfordGameMode, which plays the opening and runs the fights.

Also imports the pitchfork and the archers' bow (Art/Weapons, from Tools/make_pitchfork.py and make_bow.py).

Axes: +X is north, +Y is east, 1 unit = 1 cm; the map is 150 m north to south and 250 m west to east.

Run from the repo root after Tools/build_sword.py (it reuses the placeholder material). To rebuild, delete
Content/Maps/Marrowford.umap first (the editor won't replace a map it has loaded), then:
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_marrowford.py" -unattended -nosplash
"""
import math
import os
import random
import shutil

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FOLDER = "/Game/Conquer/Marrowford"
MAP = "/Game/Maps/Marrowford"
PLACEHOLDER = "/Game/Conquer/Weapons/M_Placeholder"
EXPOSURE = 0.6     # auto exposure held at one scene luminance (higher reads darker)

MAP_X, MAP_Y = 15000.0, 25000.0
RIVER_Y0, RIVER_Y1 = 17300.0, 19100.0      # the river runs north-south between these
BRIDGE_X0, BRIDGE_X1 = 9200.0, 9800.0      # the mill bridge crosses here
GATE = (12900.0, 22900.0)                  # the village gate, in a palisade running north-west to south-east

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
mel = unreal.MaterialEditingLibrary
rng = random.Random(7)
counts = {}


def log(msg):
    unreal.log("[build_marrowford] " + msg)


# --- Materials ----------------------------------------------------------------------------------------------------

def tinted(name, color, metallic=0.0, roughness=0.85):
    mi = tools.create_asset(name, FOLDER, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", assets.load_asset(PLACEHOLDER))
    mel.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(*color, 1))
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metallic)
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", roughness)
    assets.save_asset(mi.get_path_name(), only_if_is_dirty=False)
    return mi


def glowing(name, color, strength, translucent_opacity=None):
    """Emissive material; with an opacity it is translucent and unlit (smoke)."""
    mat = tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 0)
    glow.set_editor_property("constant", unreal.LinearColor(*[c * strength for c in color], 1))
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent_opacity is None:
        base = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -500, 200)
        base.set_editor_property("constant", unreal.LinearColor(0.05, 0.02, 0.01, 1))
        mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        opacity = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -500, 200)
        opacity.set_editor_property("r", translucent_opacity)
        mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    assets.save_asset(mat.get_path_name(), only_if_is_dirty=False)
    return mat


# --- Placing things ---------------------------------------------------------------------------------------------------

def tag(actor, *tags):
    actor.set_editor_property("tags", [unreal.Name(t) for t in tags])


def rotated(dx, dy, yaw):
    r = math.radians(yaw)
    return dx * math.cos(r) - dy * math.sin(r), dx * math.sin(r) + dy * math.cos(r)


def block(shape, location, size, material, label, yaw=0.0, roll=0.0, pitch=0.0, collide=True, hidden=False, tags=()):
    """A basic shape sized in centimetres (the basic shapes are 100 units across)."""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), unreal.Rotator(roll, pitch, yaw))
    actor.set_actor_label(label)
    comp = actor.static_mesh_component
    comp.set_static_mesh(assets.load_asset("/Engine/BasicShapes/" + shape))
    if material:
        comp.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if not collide:
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)
    if hidden:
        actor.set_actor_hidden_in_game(True)
        comp.set_editor_property("cast_shadow", False)
    if tags:
        tag(actor, *tags)
    group = label.rstrip("0123456789_")
    counts[group] = counts.get(group, 0) + 1
    return actor


def segment(a, b, width, height, z, material, label, collide=True, hidden=False):
    """A box from point a to point b (top-down), e.g. a road strip or a wall."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    length = math.hypot(dx, dy)
    yaw = math.degrees(math.atan2(dy, dx))
    return block("Cube", ((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, z), (length, width, height), material, label, yaw=yaw,
                 collide=collide, hidden=hidden)


def wall(a, b, label, height=600.0):
    """An invisible wall that keeps Wren on the route."""
    return segment(a, b, 60, height, height / 2, None, label, hidden=True)


def light(location, intensity, radius, color, label, shadows=False, tags=()):
    actor = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    actor.set_actor_label(label)
    lc = actor.point_light_component
    # Movable, so the game mode can switch a fire's light on and off (and nothing needs a lighting build).
    lc.set_mobility(unreal.ComponentMobility.MOVABLE)
    lc.set_editor_property("intensity", intensity)
    lc.set_editor_property("attenuation_radius", radius)
    lc.set_editor_property("light_color", unreal.Color(r=color[0], g=color[1], b=color[2], a=255))
    lc.set_editor_property("cast_shadows", shadows)
    if tags:
        tag(actor, *tags)
    return actor


def fire(x, y, z, size, label, tags=(), shadows=False, smoke=True):
    """Flames (glowing cones), a light, and a smoke column drifting up from them."""
    made = []
    for i in range(3):
        ox, oy = rng.uniform(-0.35, 0.35) * size, rng.uniform(-0.35, 0.35) * size
        h = size * rng.uniform(0.9, 1.6)
        made.append(block("Cone", (x + ox, y + oy, z + h / 2), (size * 0.6, size * 0.6, h), FIRE, label + "Flame",
                          yaw=rng.uniform(0, 360), collide=False, tags=tags))
    made.append(light((x, y, z + size * 1.4), size * 9.0, 700.0 + size * 6, (255, 140, 60), label + "Light",
                      shadows=shadows, tags=tags))
    if smoke:
        for i in range(3):
            s = size * (1.6 + i * 0.9)
            made.append(block("Sphere", (x + i * 60, y - i * 90, z + size * 2.2 + i * size * 1.6), (s, s, s * 1.3), SMOKE,
                              label + "Smoke", collide=False, tags=tags))
    return made


def house(cx, cy, sx, sy, yaw, label, burning=False, height=350.0, tags=()):
    """Walls, a pitched roof (a cube turned 45 degrees so its top half is the roof), a door; fire on the roof if burning."""
    made = [block("Cube", (cx, cy, height / 2), (sx, sy, height), PLASTER, label + "Walls", yaw=yaw, tags=tags)]
    roof = CHARRED if burning else THATCH
    if sx >= sy:
        side = sy * 1.12 / math.sqrt(2)
        made.append(block("Cube", (cx, cy, height), (sx * 1.06, side, side), roof, label + "Roof", yaw=yaw, roll=45, tags=tags))
    else:
        side = sx * 1.12 / math.sqrt(2)
        made.append(block("Cube", (cx, cy, height), (side, sy * 1.06, side), roof, label + "Roof", yaw=yaw, pitch=45, tags=tags))
    # Timber corner posts and a door on the +X face.
    for px, py in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
        ox, oy = rotated(px * sx / 2, py * sy / 2, yaw)
        made.append(block("Cube", (cx + ox, cy + oy, height / 2), (24, 24, height), TIMBER, label + "Post", yaw=yaw, collide=False, tags=tags))
    ox, oy = rotated(sx / 2 + 2, 0, yaw)
    made.append(block("Cube", (cx + ox, cy + oy, 110), (8, 110, 220), DARK, label + "Door", yaw=yaw, collide=False, tags=tags))
    if burning:
        # Along the ridge, which stands half the roof's width above the walls.
        ridge = height + min(sx, sy) * 0.56
        for i in range(2):
            along = rng.uniform(-0.3, 0.3)
            ox, oy = rotated(along * sx, 0, yaw) if sx >= sy else rotated(0, along * sy, yaw)
            made += fire(cx + ox, cy + oy, ridge - 60, 130, label + "Fire%d" % i, tags=tags, shadows=(i == 0))
    return made


def ridge_height(sx, sy, height=350.0):
    return height + min(sx, sy) * 0.56


def fence(a, b, label, collide=True):
    length = math.hypot(b[0] - a[0], b[1] - a[1])
    for h in (55, 100):
        segment(a, b, 8, 10, h, TIMBER, label + "Rail", collide=collide)
    posts = max(1, int(length / 300))
    for i in range(posts + 1):
        t = i / posts
        block("Cube", (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, 60), (14, 14, 120), TIMBER, label + "Post", collide=False)
    if collide:
        wall(a, b, label + "Block", 160)


def tree(x, y, label):
    h = rng.uniform(700, 1200)
    block("Cylinder", (x, y, h * 0.25), (50, 50, h * 0.5), TIMBER, label + "Trunk")
    block("Cone", (x, y, h * 0.65), (h * 0.5, h * 0.5, h * 0.75), LEAVES, label + "Crown", yaw=rng.uniform(0, 360), collide=False)


def road(points, width, label, material=None):
    for i in range(len(points) - 1):
        segment(points[i], points[i + 1], width, 4, 1, material or DIRT, "%s%d" % (label, i), collide=False)


def near_route(x, y, margin):
    for i in range(len(ROUTE) - 1):
        (ax, ay), (bx, by) = ROUTE[i], ROUTE[i + 1]
        dx, dy = bx - ax, by - ay
        t = max(0.0, min(1.0, ((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy)))
        if math.hypot(x - (ax + dx * t), y - (ay + dy * t)) < margin:
            return True
    return False


def look_at(source, target):
    dx, dy, dz = target[0] - source[0], target[1] - source[1], target[2] - source[2]
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.hypot(dx, dy)))
    return unreal.Rotator(0, pitch, yaw)


def camera(tag_name, location, target, focal=35.0):
    actor = actors.spawn_actor_from_class(unreal.CineCameraActor, unreal.Vector(*location), look_at(location, target))
    actor.set_actor_label(tag_name)
    tag(actor, tag_name)
    comp = actor.get_cine_camera_component()
    comp.set_editor_property("current_focal_length", focal)
    focus = comp.get_editor_property("focus_settings")
    focus.set_editor_property("focus_method", unreal.CameraFocusMethod.DISABLE)
    comp.set_editor_property("focus_settings", focus)
    return actor


def figure(mesh, anim, location, yaw_facing, scale, label, tags):
    """A placeholder person for the opening: a skeletal mesh looping one clip. The bodies face +Y, hence the -90."""
    actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(*location), unreal.Rotator(0, 0, yaw_facing - 90))
    actor.set_actor_label(label)
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    comp = actor.skeletal_mesh_component
    comp.set_skeletal_mesh_asset(assets.load_asset(mesh))
    comp.set_editor_property("animation_mode", unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    comp.set_editor_property("animation_data", unreal.SingleAnimationPlayData(
        anim_to_play=assets.load_asset(anim), saved_looping=True, saved_playing=True, saved_position=rng.uniform(0, 1), saved_play_rate=1.0))
    comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    tag(actor, *tags)
    return actor


def encounter(name, centre, extent, waves, spawns, hint="", boss=False, close=(), open_=()):
    actor = actors.spawn_actor_from_class(unreal.EncounterVolume, unreal.Vector(*centre), unreal.Rotator(0, 0, 0))
    actor.set_actor_label("Encounter_" + name.replace(" ", ""))
    actor.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(*extent), True)
    actor.set_editor_property("encounter_name", name)
    actor.set_editor_property("waves", [unreal.EncounterWave(count=c, enemy_class=cls or SOLDIER_CLASS, start_when_alive_at_most=at,
                                                             delay=d) for c, cls, at, d in waves])
    actor.set_editor_property("spawn_points", [unreal.Vector(x, y, 0) for x, y in spawns])
    actor.set_editor_property("start_hint", hint)
    actor.set_editor_property("boss_fight", boss)
    actor.set_editor_property("close_on_start", list(close))
    actor.set_editor_property("open_on_done", list(open_))
    return actor


def trigger(kind, order, label, centre, extent, yaw):
    actor = actors.spawn_actor_from_class(unreal.LevelTriggerVolume, unreal.Vector(*centre), unreal.Rotator(0, 0, yaw))
    actor.set_actor_label("Trigger_" + label.replace(" ", ""))
    actor.get_component_by_class(unreal.BoxComponent).set_box_extent(unreal.Vector(*extent), True)
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("order", order)
    actor.set_editor_property("label", label)
    return actor


def pickup(kind, amount, label, location, mesh, size, material):
    actor = actors.spawn_actor_from_class(unreal.ConquerPickup, unreal.Vector(*location), unreal.Rotator(0, 0, 0))
    actor.set_actor_label("Pickup_" + label.split(":")[0].replace(" ", ""))
    actor.set_editor_property("kind", kind)
    actor.set_editor_property("amount", amount)
    actor.set_editor_property("label", label)
    comp = actor.get_editor_property("mesh")
    comp.set_static_mesh(assets.load_asset("/Engine/BasicShapes/" + mesh))
    comp.set_material(0, material)
    comp.set_relative_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    return actor


def import_weapon(name, slots):
    """Imports Art/Weapons/<name>.fbx into its own folder and gives its slots our material instances."""
    folder = FOLDER + "/" + name[len("SM_"):]
    task = unreal.AssetImportTask()
    task.filename = os.path.join(REPO, "Art", "Weapons", name + ".fbx")
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    mesh = assets.load_asset(folder + "/" + name)
    if not mesh:
        raise RuntimeError(name + " import failed")
    for i, slot in enumerate(mesh.get_editor_property("static_materials")):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        match = next((m for key, m in slots.items() if slot_name.startswith(key)), None)
        if match:
            mesh.set_material(i, match)
    for asset_path in assets.list_assets(folder, recursive=False):
        # Interchange also imports the FBX's own materials; the instances above replace them.
        if assets.find_asset_data(asset_path).asset_class_path.asset_name == "Material":
            assets.delete_asset(asset_path)
    assets.save_asset(folder + "/" + name, only_if_is_dirty=False)
    return mesh


# --- Build ------------------------------------------------------------------------------------------------------------

# Clear the last build on disk before the editor loads it (deleting in the editor leaves the map in place).
shutil.rmtree(os.path.join(REPO, "Content", FOLDER[len("/Game/"):]), ignore_errors=True)
GRASS = tinted("MI_Grass", (0.07, 0.09, 0.04), 0.0, 0.95)
DIRT = tinted("MI_Dirt", (0.16, 0.11, 0.07), 0.0, 0.95)
FIELD = tinted("MI_Field", (0.13, 0.09, 0.05), 0.0, 0.95)
COBBLES = tinted("MI_Cobbles", (0.22, 0.21, 0.19), 0.0, 0.85)
PLASTER = tinted("MI_Plaster", (0.42, 0.37, 0.29), 0.0, 0.9)
TIMBER = tinted("MI_Timber", (0.17, 0.10, 0.05), 0.0, 0.85)
THATCH = tinted("MI_Thatch", (0.30, 0.23, 0.11), 0.0, 0.95)
CHARRED = tinted("MI_Charred", (0.035, 0.03, 0.025), 0.0, 0.95)
DARK = tinted("MI_Dark", (0.02, 0.015, 0.01), 0.0, 0.9)
STONE = tinted("MI_Stone", (0.28, 0.27, 0.25), 0.0, 0.9)
IRON = tinted("MI_Iron", (0.10, 0.10, 0.11), 1.0, 0.5)
LEAVES = tinted("MI_Leaves", (0.03, 0.05, 0.025), 0.0, 0.9)
WATER = tinted("MI_Water", (0.02, 0.04, 0.06), 0.0, 0.05)
FIRE = glowing("M_Fire", (1.0, 0.32, 0.05), 6.0)
SMOKE = glowing("M_Smoke", (0.05, 0.045, 0.04), 1.0, translucent_opacity=0.2)
pitchfork = import_weapon("SM_Pitchfork", {"Wood": tinted("MI_Wood", (0.30, 0.19, 0.10), 0.0, 0.8),
                                           "Iron": tinted("MI_ForkIron", (0.22, 0.22, 0.23), 1.0, 0.55)})
# The archers' bow (Tools/make_bow.py).
import_weapon("SM_Bow", {"Wood": tinted("MI_BowWood", (0.25, 0.15, 0.07), 0.0, 0.8),
                         "String": tinted("MI_BowString", (0.75, 0.72, 0.62), 0.0, 0.8)})

if assets.does_asset_exist(MAP):
    assets.delete_asset(MAP)
if assets.does_asset_exist(MAP):
    raise RuntimeError("delete Content/Maps/Marrowford.umap before running this (the editor won't replace a loaded map)")
levels.new_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/Conquer.MarrowfordGameMode"))

# Her road, start to end: farm, lane, square, bridge road, bridge, gate road, gate.
LANE = [(4300, 4600), (5200, 6000), (6200, 8000), (7000, 9800), (7500, 11000)]
BRIDGE_ROAD = [(8700, 14000), (9500, 16300), (9500, 17000)]
GATE_ROAD = [(9500, 19400), (10600, 20700), (11900, 22100), GATE, (13800, 23800)]
ROUTE = [(2600, 3300)] + LANE + [(7800, 12200)] + BRIDGE_ROAD + GATE_ROAD

# Ground: the two banks, the river bed and the water between them.
block("Cube", (MAP_X / 2, RIVER_Y0 / 2, -50), (MAP_X, RIVER_Y0, 100), GRASS, "GroundWest")
block("Cube", (MAP_X / 2, (RIVER_Y1 + MAP_Y) / 2, -50), (MAP_X, MAP_Y - RIVER_Y1, 100), GRASS, "GroundEast")
block("Cube", (MAP_X / 2, (RIVER_Y0 + RIVER_Y1) / 2, -300), (MAP_X, RIVER_Y1 - RIVER_Y0, 100), DIRT, "RiverBed")
block("Cube", (MAP_X / 2, (RIVER_Y0 + RIVER_Y1) / 2, -90), (MAP_X, RIVER_Y1 - RIVER_Y0, 4), WATER, "Water", collide=False)
for y in (RIVER_Y0, RIVER_Y1):
    block("Cube", (MAP_X / 2, y, -100), (MAP_X, 80, 200), STONE, "Bank")
    wall((0, y), (BRIDGE_X0, y), "BankWall")
    wall((BRIDGE_X1, y), (MAP_X, y), "BankWall")
# The map's edge.
for a, b in (((0, 0), (MAP_X, 0)), ((0, MAP_Y), (MAP_X, MAP_Y)), ((0, 0), (0, MAP_Y)), ((MAP_X, 0), (MAP_X, MAP_Y))):
    wall(a, b, "EdgeWall", 1500)

# Roads.
road([(2600, 3300), (3700, 4300)] + LANE, 520, "Lane")
road(BRIDGE_ROAD, 520, "BridgeRoad")
road(GATE_ROAD, 560, "GateRoad")
road([(9400, 10600), (10400, 10050)], 360, "SmithyPath")
block("Cube", (7800, 12200, 2), (3600, 3600, 4), COBBLES, "Square", collide=False)

# 1. The farm: farmhouse, barn and shed inside a fence, fields to the south and west.
house(2500, 1900, 700, 1100, 0, "Farmhouse", tags=("Farmhouse",))
house(1300, 3700, 900, 1200, 0, "Barn")
house(3600, 1100, 400, 500, 0, "Shed")
for i, (fx, fy, burn, z) in enumerate(((2500, 1700, 160, ridge_height(700, 1100)), (2500, 2200, 140, ridge_height(700, 1100)),
                                        (1300, 3500, 150, ridge_height(900, 1200)), (1300, 4000, 130, ridge_height(900, 1200)))):
    fire(fx, fy, z - 60, burn, "FarmFire%d" % i, tags=("FarmFire",), shadows=(i == 0))
for row in range(12):
    x = 450 + row * 110
    segment((x, 400), (x, 1600 if row < 6 else 1400), 50, 24, 12, FIELD, "CropRow", collide=False)
for row in range(10):
    y = 4500 + row * 120
    segment((600, y), (1900, y), 50, 24, 12, FIELD, "CropRow", collide=False)
fence((400, 300), (4200, 300), "FarmFenceS")
fence((400, 300), (400, 4800), "FarmFenceW")
fence((400, 4800), (3600, 4800), "FarmFenceE")
fence((4200, 300), (4200, 3900), "FarmFenceN")
block("Cube", (3400, 2600, 40), (120, 90, 80), TIMBER, "WoodPile")
block("Cylinder", (3200, 3900, 50), (90, 90, 100), TIMBER, "WaterBarrel")

# 2. The lane into the village, burning houses either side.
for i, (hx, hy, sx, sy, yaw, burn) in enumerate(((4500, 6300, 600, 800, 30, True), (6100, 5600, 600, 700, 30, False),
                                                  (5300, 8000, 700, 700, 25, False), (7000, 7600, 600, 900, 25, True),
                                                  (6000, 9800, 700, 800, 20, True), (7800, 9300, 600, 700, 20, False))):
    house(hx, hy, sx, sy, yaw, "LaneHouse%d" % i, burning=burn)
fence((5600, 7000), (6400, 8800), "LaneFence0", collide=False)
fence((4700, 7400), (5300, 9000), "LaneFence1", collide=False)

# 3. The burning square: houses round its edges, a well, a burning cart; the smithy up a side path to the north-west.
for i, (hx, hy, sx, sy, burn) in enumerate(((5400, 12000, 800, 1000, True), (5400, 13400, 700, 800, False),
                                             (10200, 12300, 800, 900, True), (10200, 13600, 700, 700, False),
                                             (7000, 14600, 900, 700, True), (9900, 14800, 700, 700, False),
                                             (8700, 9600, 800, 700, True))):
    house(hx, hy, sx, sy, 0, "SquareHouse%d" % i, burning=burn)
block("Cylinder", (7600, 12300, 50), (220, 220, 100), STONE, "Well")
block("Cube", (8300, 12900, 70), (300, 160, 80), TIMBER, "Cart")
for wy in (-90, 90):
    block("Cylinder", (8300, 12900 + wy, 55), (110, 110, 12), TIMBER, "CartWheel", roll=90)
fire(8300, 12900, 110, 110, "CartFire")
# The smithy: a roof on posts, an anvil and a glowing forge.
SMITHY = (10900, 9700)
for px, py in ((-400, -400), (-400, 400), (400, -400), (400, 400)):
    block("Cube", (SMITHY[0] + px, SMITHY[1] + py, 160), (30, 30, 320), TIMBER, "SmithyPost")
block("Cube", (SMITHY[0], SMITHY[1], 330), (900, 900, 30), THATCH, "SmithyRoof")
block("Cube", (SMITHY[0] + 200, SMITHY[1], 50), (220, 220, 100), STONE, "Forge")
block("Cube", (SMITHY[0] + 200, SMITHY[1], 102), (160, 160, 6), FIRE, "ForgeCoals", collide=False)
light((SMITHY[0] + 200, SMITHY[1], 220), 1500, 900, (255, 120, 40), "ForgeLight")
block("Cube", (SMITHY[0] - 150, SMITHY[1] + 150, 40), (40, 40, 80), IRON, "AnvilBase")
block("Cube", (SMITHY[0] - 150, SMITHY[1] + 150, 90), (100, 40, 22), IRON, "Anvil")
pickup(unreal.PickupKind.WHETSTONE, 1.2, "Whetstone: her blows strike harder", (SMITHY[0] - 300, SMITHY[1] + 350, 90), "Cube", (30, 12, 8), STONE)

# 4. The river, the mill and the mill bridge.
MILL = (10700, 16400)
house(MILL[0], MILL[1], 800, 800, 0, "Mill", height=450)
block("Cylinder", (MILL[0], RIVER_Y0 + 250, 50), (600, 600, 60), TIMBER, "MillWheel", roll=90, collide=False)
block("Cube", ((BRIDGE_X0 + BRIDGE_X1) / 2, (RIVER_Y0 + RIVER_Y1) / 2, 10), (BRIDGE_X1 - BRIDGE_X0, 2400, 30), TIMBER, "BridgeDeck")
for x in (BRIDGE_X0, BRIDGE_X1):
    segment((x, 17000), (x, 19400), 20, 30, 110, TIMBER, "BridgeRail")
    wall((x, 17000), (x, 19400), "BridgeRailBlock", 300)
    for y in range(17000, 19401, 400):
        block("Cube", (x, y, 50), (24, 24, 140), TIMBER, "BridgePost")
    block("Cylinder", (x, (RIVER_Y0 + RIVER_Y1) / 2, -150), (80, 80, 300), TIMBER, "BridgePier", collide=False)
# The bridge's way back: burning wreckage that drops behind her when the soldiers come.
BRIDGE_BACK = ((BRIDGE_X0 + BRIDGE_X1) / 2, 17300)
bridge_back = [block("Cube", (BRIDGE_BACK[0], BRIDGE_BACK[1], 150), (BRIDGE_X1 - BRIDGE_X0, 100, 300), CHARRED, "BridgeBlocker")]
bridge_back += fire(BRIDGE_BACK[0], BRIDGE_BACK[1], 300, 120, "BridgeBlockerFire", smoke=False)

# 5. The gate: a palisade from the river's far side to the map's east edge, a portcullis between two towers.
gate_dir = (1 / math.sqrt(2), -1 / math.sqrt(2))
gate_yaw = math.degrees(math.atan2(gate_dir[1], gate_dir[0]))
west_end, east_end = (10800, MAP_Y), (MAP_X, 20800)
gate_l = (GATE[0] - gate_dir[0] * 330, GATE[1] - gate_dir[1] * 330)
gate_r = (GATE[0] + gate_dir[0] * 330, GATE[1] + gate_dir[1] * 330)
for a, b, name in ((west_end, gate_l, "PalisadeW"), (gate_r, east_end, "PalisadeE")):
    segment(a, b, 120, 450, 225, TIMBER, name)
for i, side in enumerate((-1, 1)):
    tx, ty = GATE[0] + side * gate_dir[0] * 520, GATE[1] + side * gate_dir[1] * 520
    block("Cylinder", (tx, ty, 350), (420, 420, 700), STONE, "GateTower%d" % i)
    fire(tx, ty, 700, 70, "GateTowerTorch%d" % i, smoke=False)
block("Cube", (GATE[0], GATE[1], 510), (160, 700, 120), STONE, "GateLintel", yaw=gate_yaw)
portcullis = []
for i in range(7):
    offset = (i - 3) * 85
    portcullis.append(block("Cylinder", (GATE[0] + gate_dir[0] * offset, GATE[1] + gate_dir[1] * offset, 225), (12, 12, 450), IRON,
                            "PortcullisBar", collide=False))
for z in (90, 225, 380):
    portcullis.append(block("Cube", (GATE[0], GATE[1], z), (14, 620, 14), IRON, "PortcullisRail", yaw=gate_yaw, collide=False))
portcullis.append(block("Cube", (GATE[0], GATE[1], 225), (60, 640, 450), None, "PortcullisBlock", yaw=gate_yaw, hidden=True))
for i, (hx, hy, sx, sy, burn) in enumerate(((10000, 21800, 700, 700, True), (11800, 20600, 700, 800, False))):
    house(hx, hy, sx, sy, 45, "GateHouse%d" % i, burning=burn)

# Trees: a dark wood round the map's edge, clumps in the open, never on her road.
for i in range(170):
    t = rng.uniform(0, 1)
    side = rng.randrange(4)
    if side == 0:
        x, y = rng.uniform(150, 600), t * MAP_Y
    elif side == 1:
        x, y = MAP_X - rng.uniform(150, 600), t * MAP_Y
    elif side == 2:
        x, y = t * MAP_X, rng.uniform(150, 600)
    else:
        x, y = t * MAP_X, MAP_Y - rng.uniform(150, 600)
    if RIVER_Y0 - 200 < y < RIVER_Y1 + 200 or near_route(x, y, 1500) or (x < 4400 and y < 5000):
        continue
    tree(x, y, "EdgeTree")
placed = 0
while placed < 45:
    x, y = rng.uniform(800, MAP_X - 800), rng.uniform(800, MAP_Y - 800)
    if RIVER_Y0 - 300 < y < RIVER_Y1 + 300 or near_route(x, y, 1800) or (x < 4600 and y < 5200) \
            or (5500 < x < 10800 and 9000 < y < 15500) or x + y > 35000:
        continue
    tree(x, y, "Tree")
    placed += 1

# Night: a twilight sky with the sun just gone, a cold moon for fill, and the fires doing the rest.
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0, 4, 200))
sun.set_actor_label("SunBelowHorizon")
sun.light_component.set_editor_property("atmosphere_sun_light", True)
sun.light_component.set_editor_property("intensity", 2.0)
sun.light_component.set_editor_property("cast_shadows", False)
moon = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 2000), unreal.Rotator(0, -38, 30))
moon.set_actor_label("Moon")
moon.light_component.set_editor_property("intensity", 1.2)
moon.light_component.set_editor_property("light_color", unreal.Color(r=150, g=175, b=255, a=255))
moon.light_component.set_editor_property("atmosphere_sun_light", False)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, 0, 0))
sky.set_actor_label("SkyLight")
sky.light_component.set_editor_property("real_time_capture", True)
sky.light_component.set_editor_property("intensity", 2.5)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0)).set_actor_label("SkyAtmosphere")
fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_actor_label("SmokeHaze")
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_comp.set_editor_property("fog_density", 0.012)
fog_comp.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(0.03, 0.035, 0.05, 1))
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
post.set_actor_label("PostProcess")
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
for name, value in (("auto_exposure_min_brightness", EXPOSURE), ("auto_exposure_max_brightness", EXPOSURE),
                    ("auto_exposure_bias", 0.0)):
    settings.set_editor_property("override_" + name, True)
    settings.set_editor_property(name, value)
post.set_editor_property("settings", settings)

# The opening: Wren in the yard, her family at the farmhouse door, soldiers waiting at the end of the lane.
WREN = (2600, 3300)
actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(WREN[0], WREN[1], 100), unreal.Rotator(0, 0, 40)).set_actor_label("PlayerStart")
MANNY = "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"
QUINN = "/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple"
IDLE = "/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle"
figure(MANNY, IDLE, (2450, 2560, 0), 50, 1.0, "Father", ("Family",))
figure(QUINN, IDLE, (2650, 2520, 0), 60, 0.96, "Mother", ("Family",))
figure(MANNY, IDLE, (2560, 2700, 0), 45, 0.68, "Brother", ("Family",))
SOLDIER = "/Game/Characters/Conquer/Soldier/Soldier_rig"
WALK = "/Game/Characters/Conquer/Soldier/Anims/MF_Unarmed_Walk_Fwd_Soldier"
for i, (sx, sy) in enumerate(((4700, 5200), (4950, 5000), (5100, 5350))):
    soldier = figure(SOLDIER, WALK, (sx, sy, 0), 220, 1.0, "IntroSoldier%d" % i, ("IntroSoldier",))
    # Each carries a torch: the only light on the farm until it burns.
    flame = block("Cone", (sx + 60, sy + 60, 210), (18, 18, 40), FIRE, "IntroTorchFlame", collide=False, tags=("IntroTorch",))
    glow = light((sx + 60, sy + 60, 250), 260, 1100, (255, 150, 70), "IntroTorchLight", tags=("IntroTorch",))
    for part in (flame, glow):
        part.attach_to_actor(soldier, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD,
                             unreal.AttachmentRule.KEEP_WORLD, False)
# A lantern on a post in the yard, far enough off the walls not to burn them white.
block("Cube", (2950, 2900, 110), (14, 14, 220), TIMBER, "LanternPost")
block("Cube", (2950, 2900, 232), (18, 18, 26), FIRE, "Lantern", collide=False)
light((2950, 2900, 240), 160, 1100, (255, 170, 90), "LanternLight")
prop = block("Cube", (WREN[0] + 120, WREN[1] - 60, 3), (100, 100, 100), None, "PitchforkProp", yaw=120, collide=False, tags=("PitchforkProp",))
prop.static_mesh_component.set_static_mesh(pitchfork)
camera("IntroCam1", (900, 6000, 650), (3300, 3300, 100), focal=20.0)
camera("IntroCam2", (3150, 3250, 210), (2550, 2580, 130), focal=30.0)
camera("IntroCam3", (2150, 2750, 220), (3900, 4300, 150), focal=28.0)
camera("IntroCam4", (4300, 4400, 450), (2500, 2300, 300), focal=24.0)
camera("IntroCam5", (2250, 2600, 200), (WREN[0], WREN[1], 110), focal=28.0)

# The fights, in order, and the checkpoints before the gauntlet and the boss.
SOLDIER_CLASS = unreal.load_class(None, "/Script/Conquer.EnemySoldier")
SHIELD = unreal.load_class(None, "/Script/Conquer.ShieldSoldier")
ARCHER = unreal.load_class(None, "/Script/Conquer.ArcherSoldier")
NOW = 99   # a wave that comes in with the one before it
captain = unreal.load_class(None, "/Script/Conquer.EnemyCaptain")
encounter("The Lane", (5000, 5700, 200), (700, 700, 250),
          [(1, None, 0, 0.5), (1, None, 0, 1.5), (1, None, 0, 1.5)],
          [(6000, 7600), (6300, 8300), (5800, 7200)],
          hint="Light attack   LMB / RB|Tap Guard (F / LB) just as a blow lands to parry it|"
               "Dodge (Space / A) out of trouble, then attack to lunge")
encounter("The Burning Square", (7800, 12200, 200), (1300, 1300, 250),
          [(4, None, 0, 0.5), (1, SHIELD, 3, 1.0), (1, ARCHER, NOW, 0.5), (1, None, 2, 1.0), (1, ARCHER, NOW, 0.5)],
          [(9300, 12200), (7800, 13700), (6400, 12900), (9000, 13400), (9500, 11000), (6700, 11600), (9300, 13900)],
          hint="Heavy on a staggered, wounded soldier executes him and heals her|"
               "Guard break (E / X) throws a guarding soldier open; press it as his own grab comes in to counter")
trigger(unreal.LevelTrigger.CHECKPOINT, 1, "Before the bridge", (9500, 16300, 120), (400, 450, 200), 90)
encounter("The Mill Bridge", (9500, 17750, 200), (260, 260, 250),
          [(2, None, 0, 0.5), (1, ARCHER, NOW, 0.3), (1, SHIELD, 1, 1.0), (1, None, NOW, 0.3), (2, None, 1, 1.0),
           (1, ARCHER, NOW, 0.3)],
          [(9500, 20000), (9250, 20350), (9750, 20900), (9500, 20300), (9300, 20500), (9700, 20500), (9400, 21200)],
          hint="Every swing and dodge costs stamina: watch the yellow bar",
          close=bridge_back)
trigger(unreal.LevelTrigger.CHECKPOINT, 2, "Before the gate", (10600, 20700, 120), (400, 400, 200), 45)
pickup(unreal.PickupKind.HEALTH, 60, "Bread and water: health restored", (10350, 21150, 90), "Cylinder", (30, 30, 12), THATCH)
encounter("The Gate Captain", (11900, 22100, 200), (600, 600, 250),
          [(1, captain, 0, 0.5)],
          [(12450, 22450)],
          hint="Lock on   Tab / LT, then move the mouse to pick your guard side; a flick to his side as he strikes parries|"
               "His grab flashes blue: guard break (E / X) as it comes to counter it",
          boss=True, open_=portcullis)
trigger(unreal.LevelTrigger.LEVEL_EXIT, 0, "Out of Marrowford", (13800, 23800, 200), (450, 450, 250), 45)

levels.save_current_level()
log("saved %s with %d placed things: %s" % (MAP, sum(counts.values()), ", ".join("%s %d" % kv for kv in sorted(counts.items()))))
