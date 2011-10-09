#! /usr/bin/env python
# Convert x3d object into a blob of C code.
# Copyright (C) 2011 Paul Brook
# This code is licenced under the GNU GPL v3

import xml.dom.minidom
import sys
import os.path

class Mesh(object):
    def __init__(this):
	this.color = (0, 0, 0)
	this.ind_count = 0
	this.ind_start = -1
	this.vec = []

class Group(object):
    last_gid = -1
    @classmethod
    def nextGid(cls):
	cls.last_gid += 1
	return cls.last_gid
    def __init__(this):
	this.gid = this.nextGid()
	this.translate = (0.0, 0.0, 0.0)
	this.scale = (1.0, 1.0, 1.0)
	this.rotate = (1.0, 1.0, 1.0)
	this.subGroups = []
	this.mesh = None
	this.mod = ""

def findSingleElement(node, name):
    e = node.getElementsByTagName(name)
    if len(e) == 0:
	raise Exception("Missing '%s' element" % name);
    if len(e) != 1:
	raise Exception("Multiple '%s' elements" % name);
    return e[0]

def buildVectors(s):
    i = iter(s.split())
    while True:
	x = float(i.next())
	y = float(i.next())
	z = float(i.next())
	yield (x, y, z)

class SceneBuilder(object):
    def __init__(this, name):
	this.name = name
	this.materials = {}
	this.ind = []
	this.vec = []

    def doTransform(this, node):
	s = node.getAttribute("scale")
	name = node.getAttribute("DEF");
	g = this.newGroup(name)
	s = node.getAttribute("translation")
	g.translate = tuple(map(float, s.split()))
	# TODO: Rotation and scaling
	this.doNodes(g, node)
	return g

    def doShape(this, node):
	mesh = Mesh()
	e = findSingleElement(node, "Material")
	name = e.getAttribute("DEF")
	if name == "":
	    c = this.materials[e.getAttribute("USE")]
	else:
	    c = tuple(float(x) for x in e.getAttribute("diffuseColor").split())
	    this.materials[name] = c
	mesh.color = c
	e = findSingleElement(node, "IndexedTriangleSet")
	mesh.ind_start = len(this.ind)
	first_vec = len(this.vec)
	for s in e.getAttribute("index").split():
	    mesh.ind_count += 1
	    this.ind.append(int(s) + first_vec);
	e = findSingleElement(node, "Coordinate")
	this.vec.extend(buildVectors(e.getAttribute("point")))
	return mesh

    def doNode(this, g, node):
	if node.nodeType != node.ELEMENT_NODE:
	    return
	name = node.tagName
	if name == "Transform":
	    g.subGroups.append(this.doTransform(node))
	elif (name == "Group"):
	    g.mesh = this.doShape(findSingleElement(node, "Shape"))

    def doNodes(this, g, parent):
	for n in parent.childNodes:
	    this.doNode(g, n)

    def newGroup(this, name = ""):
	g = Group()
	if len(name) >= 2 and name[-2] == '.':
	    g.mod = name[-1];
	return g

    def readGroups(this, filename):
	f = open(filename, "rt")
	doc = xml.dom.minidom.parse(f)
	f.close()
	scene = findSingleElement(doc, "Scene")
	print scene
	this.root = this.newGroup()
	this.doNodes(this.root, scene)

    def outGroup(this, f, g, last):
	last_sub = None
	for sub in g.subGroups:
	    this.outGroup(f, sub, last_sub)
	    last_sub = sub
	f.write("static object_group group_%d = {\n" % g.gid)
	m = g.mesh
	if m is not None:
	    f.write("  .ind_count = %d,\n" % m.ind_count)
	    f.write("  .ind_start = %d,\n" % m.ind_start)
	    f.write("  .color = {%g, %g, %g},\n" % m.color)
	if last_sub is not None:
	    f.write("  .child = &group_%d,\n" % last_sub.gid)
	if last is not None:
	    f.write("  .next = &group_%d,\n" % last.gid)
	if g.mod != "":
	    f.write("  .mod = '%s',\n" % g.mod);
	f.write("  .translate = {%g, %g, %g},\n" % g.translate)
	f.write("};\n");

    def outGroups(this, filename):
	f = open(filename, "wt")
	f.write("#include <model.h>\n")
	f.write("static GLushort ind_table[%d] = {\n" % len(this.ind))
	for i in this.ind:
	    f.write("%d," % i)
	f.write("};\n");
	f.write("static GLfloat vec_table[%d] = {\n" % len(this.vec * 3))
	for v in this.vec:
	    f.write("%g,%g,%g," % v)
	f.write("};\n");
	this.outGroup(f, this.root, None)
	f.write("sprite_model model_%s = {\n" % this.name)
	f.write("  .ind = ind_table,\n");
	f.write("  .ind_count = %d,\n" % len(this.ind));
	f.write("  .vec = vec_table,\n");
	f.write("  .vec_count = %d,\n" % len(this.vec));
	f.write("  .group = &group_%d,\n" % this.root.gid)
	f.write("};\n");
	f.close

if len(sys.argv) != 3:
    print "Usage: %s [infile] [outfile]" % os.path.basename(sys.argv[0])
    exit(1)

sb = SceneBuilder(os.path.splitext(os.path.basename(sys.argv[1]))[0])
sb.readGroups(sys.argv[1])
sb.outGroups(sys.argv[2])
