#!BPY
import sys
import bpy
import os.path
import io_scene_x3d.export_x3d as exp

(p, b) = os.path.split(bpy.data.filepath)
(n, _) = os.path.splitext(b)
f = os.path.join(p, "..", n + ".x3d")
exp.save(sys.stdout, bpy.context, filepath=f, use_selection=False, use_triangulate=True)
