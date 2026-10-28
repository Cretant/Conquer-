"""Combat effects: synthesised sounds (Art/Audio/*.wav, written here from code, so they're ours to share) and the
materials for the weapon trails and sparks, imported into /Game/Conquer/FX.

Run from the repo root (re-running replaces what it made):
    UnrealEditor-Cmd.exe Conquer.uproject -run=pythonscript -script="Tools/build_fx.py" -unattended -nosplash
"""
import math
import os
import random
import struct
import wave

import unreal

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
AUDIO = os.path.join(REPO, "Art", "Audio")
FOLDER = "/Game/Conquer/FX"
RATE = 44100

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
rng = random.Random(11)


def log(msg):
    unreal.log("[build_fx] " + msg)


# --- Sounds -------------------------------------------------------------------------------------------------------

def write_wav(name, samples):
    peak = max(1e-6, max(abs(s) for s in samples))
    data = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s / peak * 0.9)) * 32767)) for s in samples)
    path = os.path.join(AUDIO, name + ".wav")
    with wave.open(path, "wb") as f:
        f.setnchannels(1)
        f.setsampwidth(2)
        f.setframerate(RATE)
        f.writeframes(data)
    return path


def envelope(t, attack, decay):
    return min(1.0, t / attack) * math.exp(-max(0.0, t - attack) / decay) if attack > 0 else math.exp(-t / decay)


def whoosh(seconds, low, high):
    """Air: noise through a low-pass whose cutoff sweeps up then down, swelling and fading."""
    out, y = [], 0.0
    n = int(seconds * RATE)
    for i in range(n):
        t = i / RATE
        sweep = math.sin(math.pi * min(1.0, t / seconds))
        cutoff = low + (high - low) * sweep
        a = 1.0 - math.exp(-2 * math.pi * cutoff / RATE)
        y += a * (rng.uniform(-1, 1) - y)
        out.append(y * sweep ** 1.5)
    return out


def ring(seconds, partials, decay, transient=0.01):
    """Struck metal: inharmonic partials decaying at their own rates, with a click of noise at the start."""
    out = []
    for i in range(int(seconds * RATE)):
        t = i / RATE
        s = sum(amp * math.sin(2 * math.pi * f * t) * math.exp(-t / (decay * d)) for f, amp, d in partials)
        if t < transient:
            s += rng.uniform(-1, 1) * (1 - t / transient)
        out.append(s)
    return out


def thump(seconds, freq, decay, noise):
    """A body blow: a falling low tone and a short burst of muffled noise."""
    out, y = [], 0.0
    for i in range(int(seconds * RATE)):
        t = i / RATE
        f = freq * (1 + 0.6 * math.exp(-t / 0.03))
        y += 0.15 * (rng.uniform(-1, 1) - y)
        out.append(math.sin(2 * math.pi * f * t) * math.exp(-t / decay) + y * noise * math.exp(-t / 0.04))
    return out


SOUNDS = {
    "S_Swing": whoosh(0.32, 300.0, 2600.0),
    "S_SwingHeavy": whoosh(0.5, 150.0, 1500.0),
    "S_Hit": [a + b for a, b in zip(thump(0.35, 85.0, 0.09, 3.0), whoosh(0.35, 1200.0, 4000.0))],
    "S_Block": ring(0.6, [(523, 1.0, 1.0), (1243, 0.7, 0.6), (2121, 0.5, 0.4), (3311, 0.3, 0.25)], 0.18, 0.006),
    "S_Parry": ring(1.2, [(880, 1.0, 1.0), (1760, 0.6, 0.8), (2637, 0.6, 0.6), (3950, 0.4, 0.5), (5274, 0.2, 0.3)], 0.45, 0.004),
    "S_Thud": thump(0.4, 65.0, 0.12, 2.0),
}

os.makedirs(AUDIO, exist_ok=True)
if assets.does_directory_exist(FOLDER):
    assets.delete_directory(FOLDER)
for name, samples in SOUNDS.items():
    task = unreal.AssetImportTask()
    task.filename = write_wav(name, samples)
    task.destination_path = FOLDER
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    log("sound %s" % name)


# --- Materials ----------------------------------------------------------------------------------------------------

def material(name):
    if assets.does_asset_exist(FOLDER + "/" + name):
        assets.delete_asset(FOLDER + "/" + name)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


# The trail: unlit and additive, its colour and fade carried in the vertex colours.
trail = material("M_Trail")
trail.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
trail.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
trail.set_editor_property("two_sided", True)
colour = mel.create_material_expression(trail, unreal.MaterialExpressionVertexColor, -600, 0)
boost = mel.create_material_expression(trail, unreal.MaterialExpressionMultiply, -350, 0)
mel.connect_material_expressions(colour, "", boost, "A")
boost.set_editor_property("const_b", 3.0)
fade = mel.create_material_expression(trail, unreal.MaterialExpressionMultiply, -350, 200)
mel.connect_material_expressions(boost, "", fade, "A")
mel.connect_material_expressions(colour, "A", fade, "B")
mel.connect_material_property(fade, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(trail)
assets.save_asset(trail.get_path_name(), only_if_is_dirty=False)

# Sparks: white-hot, glowing.
spark = material("M_Spark")
spark.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
glow = mel.create_material_expression(spark, unreal.MaterialExpressionConstant3Vector, -400, 0)
glow.set_editor_property("constant", unreal.LinearColor(40.0, 22.0, 8.0, 1))
mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(spark)
assets.save_asset(spark.get_path_name(), only_if_is_dirty=False)

log("done")
