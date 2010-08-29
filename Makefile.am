bin_PROGRAMS = chuckie

chuckie_SOURCES = chuckie.c spritedata.c leveldata.c sdl.c audio.c audio-sdl.c

chuckie_LDADD=$(SDL_LIBS)
AM_CFLAGS=$(SDL_CFLAGS) -Wall

pnd: chuckie-egg.pnd

chuckie-egg.pnd: chuckie PXML.xml chuckie.png
	mkdir -p pnd-tmp
	cp chuckie PXML.xml chuckie.png COPYING pnd-tmp
	pnd_make.sh -c -p chuckie-egg.pnd -d pnd-tmp -x PXML.xml -i chuckie.png
	rm -rf pnd-tmp
