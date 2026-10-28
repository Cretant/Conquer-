"""Renders a textured preview of each Meshy model (front and three-quarter) to Art/Previews/<Name>.png.

Run: blender -b -t 8 --python Tools/preview_meshy.py
"""
import glob
import math
import os

import bpy
from mathutils import Vector

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(REPO, "Art", "Meshy")
OUT = os.path.join(REPO, "Art", "Previews")

for name, folder in (("Wren", "Wren"), ("Soldier", "Soldier"), ("Garrick", "Garrick"), ("Sword", "Sword2"), ("Shield", "Shield")):
    files = (glob.glob(os.path.join(ART, folder, "**", "*Character_output.fbx"), recursive=True)
             or glob.glob(os.path.join(ART, folder, "**", "Meshy_AI_*.fbx"), recursive=True))
    if not files:
        continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=files[0])
    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    pts = [o.matrix_world @ Vector(c) for o in meshes for c in o.bound_box]
    lo = Vector((min(p.x for p in pts), min(p.y for p in pts), min(p.z for p in pts)))
    hi = Vector((max(p.x for p in pts), max(p.y for p in pts), max(p.z for p in pts)))
    centre, size = (lo + hi) / 2, max(hi - lo)

    sc = bpy.context.scene
    sc.render.engine = "BLENDER_EEVEE_NEXT" if "BLENDER_EEVEE_NEXT" in [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items] else "BLENDER_EEVEE"
    sc.render.resolution_x, sc.render.resolution_y = 1400, 1000
    sc.world = bpy.data.worlds.new("w")
    sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.32, 0.33, 0.36, 1)
    sc.world.node_tree.nodes["Background"].inputs[1].default_value = 1.2
    sun = bpy.data.objects.new("sun", bpy.data.lights.new("sun", "SUN"))
    sun.data.energy = 3.5
    sun.rotation_euler = (math.radians(50), 0, math.radians(30))
    sc.collection.objects.link(sun)

    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    cam.data.type = "ORTHO"
    # Ortho scale spans the wider (horizontal) side of the frame, so leave room for the height.
    cam.data.ortho_scale = size * 1.12 * 1400 / 1000
    sc.collection.objects.link(cam)
    sc.camera = cam
    # Characters face -Y in Blender after FBX import; render front and three-quarter views side by side.
    shots = []
    for i, yaw in enumerate((0, 35)):
        a = math.radians(yaw)
        cam.location = centre + Vector((math.sin(a) * size * 3, -math.cos(a) * size * 3, 0))
        cam.rotation_euler = (math.radians(90), 0, a)
        path = os.path.join(OUT, "_%s_%d.png" % (name, i))
        sc.render.filepath = path
        bpy.ops.render.render(write_still=True)
        shots.append(path)

    imgs = [bpy.data.images.load(p) for p in shots]
    w, h = imgs[0].size
    import numpy as np
    sheet = np.concatenate([np.array(im.pixels[:], dtype=np.float32).reshape(h, w, 4) for im in imgs], axis=1)
    out = bpy.data.images.new(name, w * 2, h, alpha=True)
    out.pixels = sheet.ravel()
    out.filepath_raw = os.path.join(OUT, name + ".png")
    out.file_format = "PNG"
    out.save()
    for p in shots:
        os.remove(p)
    print("PREVIEW", name, os.path.basename(files[0]), "size %.2f" % size)
