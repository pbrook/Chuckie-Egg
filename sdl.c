/* Copyright (C) 2010 Paul Brook
   This code is licenced under the GNU GPL v3 */
#include "chuckie.h"

#include "SDL.h"

static int scale = 2;
static int fullscreen = 0;
static int aspect_hack = 1;

SDL_Surface *sdlscreen;

SDLKey keys[5][6] = {
    /* right */ {SDLK_PERIOD, SDLK_RIGHT, SDLK_ESCAPE},
    /* left */  {SDLK_COMMA, SDLK_LEFT, SDLK_ESCAPE},
    /* down */  {SDLK_z, SDLK_DOWN, SDLK_ESCAPE},
    /* up */    {SDLK_a, SDLK_UP, SDLK_ESCAPE},
    /* jump */  {SDLK_SPACE, SDLK_HOME, SDLK_END, SDLK_PAGEUP,
		 SDLK_PAGEDOWN, SDLK_ESCAPE}
};

void die(const char *msg, ...)
{
  va_list va;
  va_start(va, msg);
  fprintf(stderr, "Error: ");
  vfprintf(stderr, msg, va);
  va_end(va);
  exit(1);
}

/* Read inputs, and wait until the start of the next frame.  */
void PollKeys(void)
{
  SDL_Event event;
  int i;
  int sym;
  SDLKey *keylist;

  while (1) {
      if (SDL_WaitEvent(&event) == 0) {
	  die("SDL_WaitEvent: %s\n", SDL_GetError());
      }
      switch (event.type) {
      case SDL_QUIT:
	  SDL_Quit();
	  exit(0);
	  break;
      case SDL_USEREVENT:
	  return;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
	  sym = event.key.keysym.sym;
	  switch (sym) {
	  case SDLK_ESCAPE:
	  case SDLK_q:
	      SDL_Quit();
	      exit(0);
	  default:
	      for (i = 0; i < 5; i++) {
		  keylist = &keys[i][0];
		  while (*keylist != SDLK_ESCAPE) {
		      if (event.key.keysym.sym == *keylist) {
			  if (event.key.state == SDL_PRESSED)
			      buttons |= 1 << i;
			  else
			      buttons &= ~(1 << i);
		      }
		      keylist++;
		  }
	      }
	      break;
	  }
	  break;
      }
  }
}

void RenderFrame(void)
{
  uint8_t *dest;
  uint8_t *src;
  uint8_t color;
  int x;
  int y;
  int i;
  int j;
  int dest_pitch;

  if (SDL_LockSurface(sdlscreen) > 0)
    die("SDL_LockScreen: %s\n", SDL_GetError());

  dest = sdlscreen->pixels;
  src = pixels;
  dest_pitch = sdlscreen->pitch - 320 * scale;
  for (y = 0; y < 256; y++) {
      if (aspect_hack
	  && (y < 7
	      || y == 19
	      || y == 20
	      || y == 21
	      || y == 32
	      || y == 33
	      || y >= 252
	      )) {
	  src += 160;
	  continue;
      }
      for (j = 0; j < scale; j++) {
	  for (x = 0; x < 160; x++) {
	      color = *(src++);
	      for (i = 0; i < scale; i++) {
		  *(dest++) = color;
		  *(dest++) = color;
	      }
	      dest += dest_pitch;
	  }
	  src -= 160;
      }
      src += 160;
  }
  SDL_UnlockSurface(sdlscreen);
  SDL_UpdateRect(sdlscreen, 0, 0, 0, 0);
}

#define YELLOW {0xff, 0xff, 0} /* Player, lift, big duck */
#define BLUE {0, 0xff, 0xff} /* Small duck */
#define GREEN {0, 0xff, 0} /* Floor */
#define PURPLE {0xff, 0, 0xff} /* Ladder */

Uint32 do_timer(Uint32 interval, void *param)
{
  SDL_Event event;
  event.type = SDL_USEREVENT;
  SDL_PushEvent(&event);
  return interval;
}

static void parse_args(int argc, const char *argv[])
{
    const char *p;
    int i;

    for (i = 1; i < argc; i++) {
	p = argv[i];
	if (*p == '-')
	  p++;
	if (*p >= '1' && *p <= '9')
	  scale = *p - '0';
	else if (*p == 'f')
	  fullscreen = 1;
	else if (*p == 'a')
	  aspect_hack = 0;
	else if (*p == 'h') {
	    printf("Usage: chuckie -123456789fa\n");
	    exit(0);
	}
    }
}

int main(int argc, const char *argv[])
{
  parse_args (argc, argv);
  int flags;
  int height;

  SDL_Color palette[16] = {
    {0, 0, 0}, YELLOW, PURPLE, GREEN,
    YELLOW, YELLOW, YELLOW, YELLOW, 
    BLUE, BLUE, BLUE, BLUE,
    YELLOW, YELLOW, YELLOW, YELLOW, 
  };

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == -1) {
      die("SDL_Init: %s\n", SDL_GetError());
  }

  flags = SDL_SWSURFACE;
  if (fullscreen)
    flags |= SDL_FULLSCREEN;

  height = aspect_hack ? 240 : 256;
  sdlscreen = SDL_SetVideoMode(scale * 320, scale * height, 8, flags);
  if (!sdlscreen) {
      SDL_Quit();
      fprintf(stderr, "SDL_SetVideoMode failed\n");
      return 0;
  }
  SDL_SetPalette(sdlscreen, SDL_LOGPAL | SDL_PHYSPAL, palette, 0, 16);

  SDL_ShowCursor(SDL_DISABLE);

  init_sound();

  SDL_AddTimer(30, do_timer, NULL);

  run_game();

  return 0;
}

