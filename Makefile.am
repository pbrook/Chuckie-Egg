bin_PROGRAMS = chuckie aachuckie glchuckie 3dchuckie

AM_CFLAGS=-Wall

noinst_LIBRARIES = libchuckie.a
libchuckie_a_SOURCES = chuckie.c leveldata.c audio.c

chuckie_SOURCES = spritedata.c sdl.c audio-sdl.c raster_4bit.c sdl-input.c
chuckie_LDADD=$(SDL_LIBS) libchuckie.a
chuckie_CFLAGS=$(SDL_CFLAGS) $(AM_CFLAGS)

glchuckie_SOURCES = spritedata.c gl.c audio-sdl.c sdl-input.c
glchuckie_LDADD=$(SDL_LIBS) libchuckie.a -lGL
glchuckie_CFLAGS=$(SDL_CFLAGS) $(AM_CFLAGS)

3dchuckie_SOURCES = spritedata.c gl2.c audio-sdl.c sdl-input.c \
	models/farmer.x3d models/duck.x3d models/egg.x3d models/wall.x3d \
	models/ladder.x3d models/grain.x3d
3dchuckie_LDADD=$(SDL_LIBS) libchuckie.a -lGL
3dchuckie_CFLAGS=$(SDL_CFLAGS) $(AM_CFLAGS)

aachuckie_SOURCES = ncurses.c
aachuckie_LDADD=libchuckie.a -lcurses

CLEANFILES = models/*.c

pnd: chuckie-egg.pnd

models/*.c: build_model.py Makefile

SUFFIXES = .x3d

.x3d.c:
	$(srcdir)/build_model.py $< $@

chuckie-egg.pnd: chuckie PXML.xml chuckie.png
	mkdir -p pnd-tmp
	cp chuckie PXML.xml chuckie.png COPYING pnd-tmp
	pnd_make.sh -c -p chuckie-egg.pnd -d pnd-tmp -x PXML.xml -i chuckie.png
	rm -rf pnd-tmp
