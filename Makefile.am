bin_PROGRAMS = chuckie aachuckie

AM_CFLAGS=-Wall

noinst_LIBRARIES = libchuckie.a
libchuckie_a_SOURCES = chuckie.c leveldata.c audio.c

chuckie_SOURCES = spritedata.c sdl.c audio-sdl.c \
		  raster_4bit.c
chuckie_LDADD=$(SDL_LIBS) libchuckie.a
chuckie_CFLAGS=$(SDL_CFLAGS)

aachuckie_SOURCES = ncurses.c
aachuckie_LDADD=libchuckie.a -lcurses

pnd: chuckie-egg.pnd

chuckie-egg.pnd: chuckie PXML.xml chuckie.png
	mkdir -p pnd-tmp
	cp chuckie PXML.xml chuckie.png COPYING pnd-tmp
	pnd_make.sh -c -p chuckie-egg.pnd -d pnd-tmp -x PXML.xml -i chuckie.png
	rm -rf pnd-tmp
