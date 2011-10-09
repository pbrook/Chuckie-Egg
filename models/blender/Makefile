MODELS = duck egg farmer grain ladder lift wall

X3D = $(patsubst %,../%.x3d,$(MODELS))

all: $(X3D)

../%.x3d: %.blend Makefile blender_export.py
	rm -f $@
	blender $< -b -P blender_export.py
